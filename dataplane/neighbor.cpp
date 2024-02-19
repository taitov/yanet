#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>

#include "dataplane.h"
#include "neighbor.h"
#include "worker.h"

using namespace dataplane::neighbor;

static void parse_rt_attributes(rtattr* rt_attributes[],
                                unsigned int rt_attributes_size,
                                rtattr* rta,
                                int length)
{
	memset(rt_attributes, 0, sizeof(rtattr*) * (rt_attributes_size + 1));
	while (RTA_OK(rta, length))
	{
		if ((rta->rta_type <= rt_attributes_size) &&
		    (!rt_attributes[rta->rta_type]))
		{
			rt_attributes[rta->rta_type] = rta;
		}

		rta = RTA_NEXT(rta, length);
	}

	if (length)
	{
		YANET_LOG_WARNING("invalid length: %u of %u\n", length, rta->rta_len);
	}
}

static std::string iface_id_to_name(unsigned int iface_id)
{
	char buffer[IFNAMSIZ];
	if (if_indextoname(iface_id, buffer) == nullptr)
	{
		snprintf(buffer, IFNAMSIZ, "unknown_%u", iface_id);
	}
	return buffer;
}

static void netlink_parse(const std::function<void(const std::string&, const common::ip_address_t&, const common::mac_address_t&)>& callback,
                          const char* buffer,
                          const unsigned int buffer_length)
{
	rtattr* rt_attributes[NDA_MAX + 1];

	unsigned int offset = 0;
	while (offset + sizeof(nlmsghdr) <= buffer_length)
	{
		nlmsghdr* nl_message_header = (nlmsghdr*)(buffer + offset);
		uint32_t length = nl_message_header->nlmsg_len;

		ndmsg* nl_message = (ndmsg*)NLMSG_DATA(nl_message_header);
		parse_rt_attributes(rt_attributes,
		                    NDA_MAX,
		                    (rtattr*)(((char*)(nl_message)) + NLMSG_ALIGN(sizeof(ndmsg))),
		                    nl_message_header->nlmsg_len - NLMSG_LENGTH(sizeof(*nl_message)));

		if (rt_attributes[NDA_DST] &&
		    rt_attributes[NDA_LLADDR] &&
		    nl_message->ndm_ifindex)
		{
			std::string interface_name = iface_id_to_name(nl_message->ndm_ifindex);

			unsigned int family = nl_message->ndm_family;
			if (family == AF_BRIDGE)
			{
				if (RTA_PAYLOAD(rt_attributes[NDA_DST]) == sizeof(in6_addr))
				{
					family = AF_INET6;
				}
				else
				{
					family = AF_INET;
				}
			}

			common::ip_address_t ip_address;
			if (family == AF_INET)
			{
				ip_address = common::ipv4_address_t(rte_be_to_cpu_32(*(const uint32_t*)RTA_DATA(rt_attributes[NDA_DST])));
			}
			else
			{
				ip_address = common::ipv6_address_t((const uint8_t*)RTA_DATA(rt_attributes[NDA_DST]));
			}

			common::mac_address_t mac_address((const uint8_t*)RTA_DATA(rt_attributes[NDA_LLADDR]));

			callback(interface_name, ip_address, mac_address);
		}

		offset += NLMSG_ALIGN(length);
	}

	if (buffer_length - offset)
	{
		YANET_LOG_WARNING("extra buffer_length: %u of %u\n", offset, buffer_length);
	}
}

static void netlink_neighbor_monitor(const std::function<void(const std::string&, const common::ip_address_t&, const common::mac_address_t&)>& callback)
{
	int nl_socket = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (nl_socket < 0)
	{
		YANET_LOG_ERROR("socket(): %s\n", strerror(errno));
		return;
	}

	sockaddr_nl nl_sockaddr;
	memset(&nl_sockaddr, 0, sizeof(nl_sockaddr));
	nl_sockaddr.nl_family = AF_NETLINK;
	nl_sockaddr.nl_groups = 1u << (RTNLGRP_NEIGH - 1);

	if (bind(nl_socket, (sockaddr*)&nl_sockaddr, sizeof(nl_sockaddr)) < 0)
	{
		YANET_LOG_ERROR("bind(): %s\n", strerror(errno));
		return;
	}

	char buffer[4096];
	for (;;)
	{
		int buffer_length = recv(nl_socket, buffer, sizeof(buffer), 0);
		if (buffer_length > 0)
		{
			netlink_parse(callback, buffer, buffer_length);
		}
		else
		{
			break;
		}
	}

	close(nl_socket);
}

module::module() :
        dataplane(nullptr)
{
	memset(&stats, 0, sizeof(stats));
}

eResult module::init(cDataPlane* dataplane)
{
	this->dataplane = dataplane;

	generation_hashtable.fill([&](neighbor::generation_hashtable& hashtable) {
		for (const auto socket_id : dataplane->get_socket_ids())
		{
			dataplane->hugepage_create_dynamic<dataplane::neighbor::hashtable>(socket_id,
			                                                                   64 * 1024,
			                                                                   hashtable.hashtable_updater[socket_id]);
		}
	});

	threads.emplace_back([this]() {
		main_thread();
	});

	threads.emplace_back([this]() {
		netlink_thread();
	});

	return eResult::success;
}

void module::update_worker_base(const std::vector<std::tuple<tSocketId, dataplane::base::generation*>>& base_nexts)
{
	auto lock = generation_hashtable.current_lock_guard();
	for (auto& [socket_id, base_next] : base_nexts)
	{
		const auto& hashtable_updater = generation_hashtable.current().hashtable_updater;
		base_next->neighbor_hashtable = hashtable_updater.find(socket_id)->second.get_pointer();
	}
}

common::neighbor::idp::show module::neighbor_show() const
{
	common::neighbor::idp::show response;

	generation_interface.current_lock();
	auto interface_id_to_name = generation_interface.current().interface_id_to_name;
	generation_interface.current_unlock();

	{
		auto lock = generation_hashtable.current_lock_guard();

		const auto& hashtable_updater = generation_hashtable.current().hashtable_updater.begin()->second;
		for (auto iter : hashtable_updater.range())
		{
			if (iter.is_valid())
			{
				auto& key = *iter.key();
				auto& value = *iter.value();

				const auto& interface_id_to_name = generation_interface.current().interface_id_to_name;
				auto it = interface_id_to_name.find(key.interface_id);
				if (it == interface_id_to_name.end())
				{
					continue;
				}

				const auto& [route_name, interface_name] = it->second;

				std::optional<uint32_t> last_update_timestamp;
				if (!(value.flags & flag_is_static))
				{
					last_update_timestamp = dataplane->get_current_time() - value.last_update_timestamp;
				}

				response.emplace_back(route_name,
				                      interface_name,
				                      common::ip_address_t(key.flags & flag_is_ipv6 ? 6 : 4, key.address.bytes),
				                      common::mac_address_t(value.ether_address.addr_bytes),
				                      last_update_timestamp);
			}
		}
	}

	return response;
}

eResult module::neighbor_insert(const common::neighbor::idp::insert& request)
{
	const auto& [route_name, interface_name, ip_address, mac_address] = request;
	(void)route_name; ///< @todo

	tInterfaceId interface_id = 0;
	{
		auto lock = generation_interface.current_lock_guard();

		const auto& interface_name_to_id = generation_interface.current().interface_name_to_id;
		auto it = interface_name_to_id.find(interface_name);
		if (it == interface_name_to_id.end())
		{
			return eResult::invalidInterfaceName;
		}

		interface_id = it->second;
	}

	dataplane::neighbor::key key;
	memset(&key, 0, sizeof(key));
	key.interface_id = interface_id;
	key.flags = 0;

	if (ip_address.is_ipv4())
	{
		key.address.mapped_ipv4_address = ipv4_address_t::convert(ip_address.get_ipv4());
	}
	else
	{
		key.address = ipv6_address_t::convert(ip_address.get_ipv6());
		key.flags |= flag_is_ipv6;
	}

	dataplane::neighbor::value value;
	memcpy(value.ether_address.addr_bytes, mac_address.data(), 6);
	value.flags = 0;
	value.flags |= flag_is_static;
	value.last_update_timestamp = dataplane->get_current_time();

	auto response = generation_hashtable.update([this, key, value](neighbor::generation_hashtable& hashtable) {
		eResult result = eResult::success;
		for (auto& [socket_id, hashtable_updater] : hashtable.hashtable_updater)
		{
			(void)socket_id;
			if (!hashtable_updater.get_pointer()->insert_or_update(key, value))
			{
				result = eResult::isFull;
				stats.hashtable_insert_error++;
			}
			else
			{
				stats.hashtable_insert_success++;
			}
		}
		return result;
	});

	return response;
}

eResult module::neighbor_remove(const common::neighbor::idp::remove& request)
{
	const auto& [route_name, interface_name, ip_address] = request;
	(void)route_name; ///< @todo

	tInterfaceId interface_id = 0;
	{
		auto lock = generation_interface.current_lock_guard();

		const auto& interface_name_to_id = generation_interface.current().interface_name_to_id;
		auto it = interface_name_to_id.find(interface_name);
		if (it == interface_name_to_id.end())
		{
			return eResult::invalidInterfaceName;
		}

		interface_id = it->second;
	}

	dataplane::neighbor::key key;
	memset(&key, 0, sizeof(key));
	key.interface_id = interface_id;
	key.flags = 0;

	if (ip_address.is_ipv4())
	{
		key.address.mapped_ipv4_address = ipv4_address_t::convert(ip_address.get_ipv4());
	}
	else
	{
		key.address = ipv6_address_t::convert(ip_address.get_ipv6());
		key.flags |= flag_is_ipv6;
	}

	auto response = generation_hashtable.update([this, key](neighbor::generation_hashtable& hashtable) {
		eResult result = eResult::success;
		for (auto& [socket_id, hashtable_updater] : hashtable.hashtable_updater)
		{
			(void)socket_id;
			if (!hashtable_updater.get_pointer()->remove(key))
			{
				result = eResult::invalidArguments;
				stats.hashtable_remove_error++;
			}
			else
			{
				stats.hashtable_remove_success++;
			}
		}
		return result;
	});

	return response;
}

eResult module::neighbor_clear()
{
	auto response = generation_hashtable.update([](neighbor::generation_hashtable& hashtable) {
		for (auto& [socket_id, hashtable_updater] : hashtable.hashtable_updater)
		{
			(void)socket_id;
			hashtable_updater.get_pointer()->clear();
		}
		return eResult::success;
	});

	return response;
}

eResult module::neighbor_update_interfaces(const common::neighbor::idp::update_interfaces& request)
{
	auto& generation = generation_interface.next();
	for (const auto& [interface_id,
	                  route_name,
	                  interface_name] : request)
	{
		generation.interface_name_to_id[interface_name] = interface_id;
		generation.interface_id_to_name[interface_id] = {route_name, interface_name};
	}

	return eResult::success;
}

common::neighbor::idp::stats module::neighbor_stats() const
{
	return stats;
}

void module::report(nlohmann::json& json)
{
	json["neighbor"]["hashtable_insert_success"] = stats.hashtable_insert_success;
	json["neighbor"]["hashtable_insert_error"] = stats.hashtable_insert_error;
	json["neighbor"]["hashtable_remove_success"] = stats.hashtable_remove_success;
	json["neighbor"]["hashtable_remove_error"] = stats.hashtable_remove_error;
	json["neighbor"]["netlink_neighbor_update"] = stats.netlink_neighbor_update;
	json["neighbor"]["resolve"] = stats.resolve;
}

void module::update_before(const common::idp::update::request& request)
{
	if (!request.neighbor())
	{
		return;
	}

	/// @todo: lock generation_interface only on `update_interfaces`
	generation_interface.next_lock();

	/// @todo: lock generation_hashtable only on write to hashtable
	generation_hashtable.next_lock();
}

void module::update(const common::idp::update::request& request,
                    common::idp::update::response& response)
{
	if (!request.neighbor())
	{
		return;
	}

	const auto& [type, variant] = *request.neighbor();

	auto& response_neighbor = response.neighbor();
	if (type == common::neighbor::idp::type::show)
	{
		response_neighbor = neighbor_show();
	}
	else if (type == common::neighbor::idp::type::insert)
	{
		response_neighbor = neighbor_insert(std::get<common::neighbor::idp::insert>(variant));
	}
	else if (type == common::neighbor::idp::type::remove)
	{
		response_neighbor = neighbor_remove(std::get<common::neighbor::idp::remove>(variant));
	}
	else if (type == common::neighbor::idp::type::clear)
	{
		response_neighbor = neighbor_clear();
	}
	else if (type == common::neighbor::idp::type::update_interfaces)
	{
		response_neighbor = neighbor_update_interfaces(std::get<common::neighbor::idp::update_interfaces>(variant));
		generation_interface.switch_generation_without_gc();
	}
	else if (type == common::neighbor::idp::type::stats)
	{
		response_neighbor = neighbor_stats();
	}
	else
	{
		response_neighbor = eResult::invalidType;
	}

	generation_hashtable.switch_generation_without_gc();
}

void module::update_after(const common::idp::update::request& request)
{
	if (!request.neighbor())
	{
		return;
	}

	generation_interface.next().interface_name_to_id.clear();
	generation_interface.next().interface_id_to_name.clear();
	generation_hashtable.next_apply();

	generation_interface.next_unlock();
	generation_hashtable.next_unlock();
}

void module::main_thread()
{
	std::vector<dataplane::neighbor::key> keys;

	for (;;)
	{
		keys.clear();

		for (auto* worker : dataplane->get_workers())
		{
			dataplane->run_on_worker_gc(worker->socketId, [&]() {
				for (auto iter : worker->neighbor_resolve.range())
				{
					iter.lock();
					if (!iter.is_valid())
					{
						iter.unlock();
						continue;
					}

					auto key = *iter.key();

					iter.unset_valid();
					iter.unlock();

					keys.emplace_back(key);
				}

				return true;
			});
		}

		for (const auto& key : keys)
		{
			resolve(key);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void module::netlink_thread()
{
#ifdef CONFIG_YADECAP_AUTOTEST
	return;
#endif // CONFIG_YADECAP_AUTOTEST

	for (;;)
	{
		netlink_neighbor_monitor([&](const std::string& interface_name,
		                             const common::ip_address_t& ip_address,
		                             const common::mac_address_t& mac_address) {
			common::neighbor::idp::insert insert("", ///< @todo: route_name
			                                     interface_name,
			                                     ip_address,
			                                     mac_address);
			common::idp::update::request update;
			update.neighbor() = {common::neighbor::idp::type::insert, insert};

			dataplane->controlPlane->update(update);
		});

		YANET_LOG_WARNING("restart neighbor_monitor\n");
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void module::resolve(const dataplane::neighbor::key& key)
{
	stats.resolve++;

	common::ip_address_t ip_address(key.flags & flag_is_ipv6 ? 6 : 4, key.address.bytes);
	std::string interface_name;
	{
		auto lock = generation_interface.current_lock_guard();
		const auto& interface_id_to_name = generation_interface.current().interface_id_to_name;
		auto it = interface_id_to_name.find(key.interface_id);
		if (it == interface_id_to_name.end())
		{
			YANET_LOG_ERROR("unknown interface_id: %u [ipv4_address: %s]\n",
			                key.interface_id,
			                ip_address.toString().data());
			return;
		}

		const auto& [it_route_name, it_interface_name] = it->second;
		(void)it_route_name;

		interface_name = it_interface_name;
	}

	YANET_LOG_DEBUG("resolve: %s, %s\n",
	                interface_name.data(),
	                ip_address.toString().data());

#ifdef CONFIG_YADECAP_AUTOTEST
	common::mac_address_t mac_address;
	mac_address.data()[0] = 44;
	mac_address.data()[1] = 44;
	if (key.flags & flag_is_ipv6)
	{
		mac_address.data()[0] = 66;
		mac_address.data()[1] = 66;
	}
	*((uint32_t*)&mac_address.data()[2]) = rte_hash_crc(key.address.bytes, 16, 0);

	common::neighbor::idp::insert insert("", ///< @todo: route_name
	                                     interface_name,
	                                     ip_address,
	                                     mac_address);
	common::idp::update::request update;
	update.neighbor() = {common::neighbor::idp::type::insert, insert};

	dataplane->controlPlane->update(update);
#else // CONFIG_YADECAP_AUTOTEST

	/// @todo: in first, try resolve like 'ip neig show 10.0.0.1 dev eth0'

	int family = AF_INET;
	int protocol = IPPROTO_ICMP;
	if (key.flags & flag_is_ipv6)
	{
		family = AF_INET6;
		protocol = IPPROTO_ICMPV6;
	}

	int icmp_socket = socket(family, SOCK_RAW, protocol);
	if (icmp_socket == -1)
	{
		YANET_LOG_WARNING("neighbor_resolve: socket(): %s\n",
		                  strerror(errno));
		return;
	}

	int rc = setsockopt(icmp_socket,
	                    SOL_SOCKET,
	                    SO_BINDTODEVICE,
	                    interface_name.data(),
	                    strlen(interface_name.data()) + 1);
	if (rc == -1)
	{
		YANET_LOG_WARNING("neighbor_resolve: setsockopt(%s): %s\n",
		                  interface_name.data(),
		                  strerror(errno));
		close(icmp_socket);
		return;
	}

	union
	{
		sockaddr address;
		sockaddr_in address_v4;
		sockaddr_in6 address_v6;
	};

	socklen_t address_length = sizeof(address_v4);

	if (key.flags & flag_is_ipv6)
	{
		address_v6.sin6_family = AF_INET6;
		address_v6.sin6_port = 0;
		memcpy(address_v6.sin6_addr.__in6_u.__u6_addr8, key.address.bytes, 16);

		address_length = sizeof(address_v6);
	}
	else
	{
		address_v4.sin_family = AF_INET;
		address_v4.sin_port = 0;
		address_v4.sin_addr.s_addr = key.address.mapped_ipv4_address.address;
	}

	icmphdr header;
	memset(&header, 0, sizeof(header));
	if (sendto(icmp_socket,
	           &header,
	           sizeof(header),
	           0,
	           &address,
	           address_length) == -1)
	{
		YANET_LOG_WARNING("neighbor_resolve: sendto(): %s\n",
		                  strerror(errno));
	}

	close(icmp_socket);
#endif // CONFIG_YADECAP_AUTOTEST
}
