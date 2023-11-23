#include "neighbor.h"
#include "controlplane.h"
#include "dataplane.h"

using namespace dataplane::neighbor;

neighbor::neighbor()
{
}

eResult neighbor::init(cDataPlane* dataplane)
{
	this->dataplane = dataplane;

	generations.apply([&](generation& generation) {
		generation.init(dataplane);
	});

	return eResult::success;
}

common::idp::neighbor::response neighbor::update(const common::idp::neighbor::request& request)
{
	const auto& [request_type, request_data] = request;

	YADECAP_LOG_DEBUG("neighbor request: %d\n", (int)request_type);

	if (request_type == common::idp::neighbor::request_type::get)
	{
		generations.current_lock();
		generations.current_unlock();
	}
	else if (request_type == common::idp::neighbor::request_type::update)
	{
		generations.apply([&](generation& generation) {
			for (auto& [socket_id, socket] : generation.sockets)
			{
				socket.
			}
		});
	}

	return eResult::success;
}

void neighbor::insert(const tInterfaceId interface_id,
                      const common::ip_address_t address,
                      const common::mac_address_t& mac_address)
{
	std::lock_guard<std::mutex> requests_lock(requests_mutex);
	requests.emplace_back(common::idp::neighbor::request_type::insert,
	                      common::idp::neighbor::insert::request(interface_id, address, mac_address));
}

void neighbor::switch_generation(const std::map<tSocketId, dataplane::base::generation*>& worker_generation)
{
	generations.next_lock();
	generations.switch_generation_without_clear();
	generations.next().requests.clear();
	generations.next().update(generations.current().requests);
	generations.next_unlock();

	generations.current_lock();
	worker_generation.find(0)->second->globalBase = NULL; ///< XXX
	generations.current_unlock();
}

void neighbor::thread()
{
	std::lock_guard<std::mutex> requests_lock(requests_mutex);
	for (const auto& request : requests)
	{
	}
	requests.clear();
}

void neighbor::monitor_thread()
{
	insert(1, {"200.0.0.1"}, {"00:11:22:33:44:55"});
}

generation::generation()
{
}

eResult socket::init(cDataPlane* dataplane,
                     const tSocketId socket_id)
{
	auto* v4 = dataplane->hugepage_create_dynamic<hashtable_v4>(socket_id, 1024, updater_v4);
	if (!v4)
	{
		return eResult::errorAllocatingMemory;
	}

	auto* v6 = dataplane->hugepage_create_dynamic<hashtable_v6>(socket_id, 1024, updater_v6);
	if (!v6)
	{
		return eResult::errorAllocatingMemory;
	}

	return eResult::success;
}

void socket::insert(const common::idp::neighbor::insert::request& request)
{
	const auto& [interface_id, address, mac_address] = request;

	value value;
	memcpy(value.ether_address.addr_bytes, mac_address.data(), 6);

	if (address.is_ipv4())
	{
		key_v4 key;
		key.interface_id = interface_id;
		key.address = ipv4_address_t::convert(address.get_ipv4());

		if (!updater_v4.hashtable->insert_or_update(key, value))
		{
			/// XXX
		}
	}
	else
	{
		key_v6 key;
		key.interface_id = interface_id;
		key.address = ipv6_address_t::convert(address.get_ipv6());

		if (!updater_v6.hashtable->insert_or_update(key, value))
		{
			/// XXX
		}
	}
}

eResult generation::init(cDataPlane* dataplane)
{
	eResult result = eResult::success;

	for (const auto socket_id : dataplane->get_socket_ids())
	{
		result = sockets[socket_id].init(dataplane, socket_id);
		if (result != eResult::success)
		{
			return result;
		}
	};

	return result;
}
