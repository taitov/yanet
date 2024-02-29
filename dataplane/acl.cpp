#include "acl.h"
#include "dataplane.h"
#include "worker.h"

using namespace dataplane::acl;

base::base(memory_manager* memory_manager,
           const tSocketId socket_id) :
        network_ipv4_source("acl.network.v4.source.lpm", memory_manager, socket_id),
        network_ipv4_destination("acl.network.v4.destination.lpm", memory_manager, socket_id),
        network_ipv6_source("acl.network.v6.source.lpm", memory_manager, socket_id),
        network_ipv6_destination_ht("acl.network.v6.destination.ht", memory_manager, socket_id),
        network_ipv6_destination("acl.network.v6.destination.lpm", memory_manager, socket_id),
        network_table("acl.network.ht", memory_manager, socket_id),
        transport_layers("acl.transport.layers", memory_manager, socket_id),
        transport_table("acl.transport.ht", memory_manager, socket_id),
        total_table("acl.total.ht", memory_manager, socket_id),
        values("acl.values", memory_manager, socket_id)
{
}

module::module() :
        dataplane(nullptr)
{
}

eResult module::init(cDataPlane* dataplane)
{
	eResult result = eResult::success;

	this->dataplane = dataplane;

	generations.fill([&](acl::generation& generation) {
		for (const auto socket_id : dataplane->get_socket_ids())
		{
			generation.bases.try_emplace(socket_id, &dataplane->memory_manager, socket_id);
		}
	});

	/// initial allocate objects
	acl_update({});

	return result;
}

void module::update_worker_base(const std::vector<std::tuple<tSocketId, dataplane::base::generation*>>& worker_base_nexts)
{
	auto lock = generations.current_lock_guard();

	const auto& generation = generations.current();
	for (auto& [socket_id, worker_base] : worker_base_nexts)
	{
		const auto& base = generation.bases.find(socket_id)->second;

		worker_base->acl_network_ipv4_source = base.network_ipv4_source.pointer;
		worker_base->acl_network_ipv4_destination = base.network_ipv4_destination.pointer;
		worker_base->acl_network_ipv6_source = base.network_ipv6_source.pointer;
		worker_base->acl_network_ipv6_destination_ht = base.network_ipv6_destination_ht.pointer;
		worker_base->acl_network_ipv6_destination = base.network_ipv6_destination.pointer;
		worker_base->acl_network_table = base.network_table.pointer;
		worker_base->acl_transport_layers = base.transport_layers.pointer;
		worker_base->acl_transport_table = base.transport_table.pointer;
		worker_base->acl_total_table = base.total_table.pointer;
		worker_base->acl_values = base.values.pointer;
	}
}

void module::report(nlohmann::json& json)
{
	auto lock = generations.current_lock_guard();

	const auto& generation = generations.current();
	for (const auto socket_id : dataplane->get_socket_ids())
	{
		const auto& base = generation.bases.find(socket_id)->second;

		base.network_ipv4_source.report(json["acl"]["network"]["ipv4"]["source"]);
		base.network_ipv4_destination.report(json["acl"]["network"]["ipv4"]["destination"]);
		base.network_ipv6_source.report(json["acl"]["network"]["ipv6"]["source"]);
		base.network_ipv6_destination_ht.report(json["acl"]["network"]["ipv6"]["destination_ht"]);
		base.network_ipv6_destination.report(json["acl"]["network"]["ipv6"]["destination"]);
		base.network_table.report(json["acl"]["network_table"]);
		base.transport_layers.report(json["acl"]["transport_layers"]);
		base.transport_table.report(json["acl"]["transport_table"]);
		base.total_table.report(json["acl"]["total_table"]);
		base.values.report(json["acl"]["values"]);
	}
}

void module::limits(common::idp::limits::response& response)
{
	auto lock = generations.current_lock_guard();

	const auto& generation = generations.current();
	for (const auto socket_id : dataplane->get_socket_ids())
	{
		const auto& base = generation.bases.find(socket_id)->second;

		base.network_ipv4_source.limits(response);
		base.network_ipv4_destination.limits(response);
		base.network_ipv6_source.limits(response);
		base.network_ipv6_destination_ht.limits(response);
		base.network_ipv6_destination.limits(response);
		base.network_table.limits(response);
		base.transport_layers.limits(response);
		base.transport_table.limits(response);
		base.total_table.limits(response);
		base.values.limits(response);
	}
}

void module::update_before(const common::idp::update::request& request)
{
	if (!request.acl())
	{
		return;
	}

	generations.next_lock();
}

void module::update(const common::idp::update::request& request,
                    common::idp::update::response& response)
{
	if (!request.acl())
	{
		return;
	}

	response.acl() = acl_update(*request.acl());
	return;
}

void module::update_after(const common::idp::update::request& request)
{
	if (!request.acl())
	{
		return;
	}

	auto& generation = generations.next();
	for (const auto socket_id : dataplane->get_socket_ids())
	{
		auto& base = generation.bases.find(socket_id)->second;

		base.network_ipv4_source.clear();
		base.network_ipv4_destination.clear();
		base.network_ipv6_source.clear();
		base.network_ipv6_destination_ht.clear();
		base.network_ipv6_destination.clear();
		base.network_table.clear();
		base.transport_layers.clear();
		base.transport_table.clear();
		base.total_table.clear();
		base.values.clear();
	}

	generations.next_unlock();
}

eResult module::acl_update(const common::acl::idp::request& request_acl)
{
	eResult result = eResult::success;
	const auto& [request_network_ipv4_source,
	             request_network_ipv4_destination,
	             request_network_ipv6_source,
	             request_network_ipv6_destination_ht,
	             request_network_ipv6_destination,
	             request_network_table,
	             request_transport_layers,
	             request_transport_table,
	             request_total_table,
	             request_values] = request_acl;

	auto& generation = generations.next();
	for (const auto socket_id : dataplane->get_socket_ids())
	{
		auto& base = generation.bases.find(socket_id)->second;

		result = base.network_ipv4_source.update(request_network_ipv4_source);
		if (result != eResult::success)
		{
			return result;
		}

		result = base.network_ipv4_destination.update(request_network_ipv4_destination);
		if (result != eResult::success)
		{
			return result;
		}

		result = base.network_ipv6_source.update(request_network_ipv6_source);
		if (result != eResult::success)
		{
			return result;
		}

		{
			std::vector<std::tuple<ipv6_address_t, tAclGroupId>> request_convert;
			for (const auto& [address, group_id] : request_network_ipv6_destination_ht)
			{
				request_convert.emplace_back(ipv6_address_t::convert(address), group_id);
			}

			result = base.network_ipv6_destination_ht.update(request_convert);
			if (result != eResult::success)
			{
				return result;
			}
		}

		result = base.network_ipv6_destination.update(request_network_ipv6_destination);
		if (result != eResult::success)
		{
			return result;
		}

		result = base.network_table.update(request_network_table);
		if (result != eResult::success)
		{
			return result;
		}

		/// update transport_layers
		{
			result = base.transport_layers.create(request_transport_layers.size());
			if (result != eResult::success)
			{
				return result;
			}

			for (unsigned int layer_id = 0;
			     layer_id < request_transport_layers.size();
			     layer_id++)
			{
				auto& transport_layer = base.transport_layers[layer_id];

				const auto& [protocol,
				             tcp_source,
				             tcp_destination,
				             tcp_flags,
				             udp_source,
				             udp_destination,
				             icmp_type_code,
				             icmp_identifier] = request_transport_layers[layer_id];

				flat<uint8_t>::updater updater_protocol; ///< @todo
				result = transport_layer.protocol.update(updater_protocol, protocol);
				if (result != eResult::success)
				{
					YANET_LOG_ERROR("acl.transport_layer.protocol.update(): %s\n", result_to_c_str(result));
					return result;
				}

				flat<uint16_t>::updater updater_tcp_source; ///< @todo
				result = transport_layer.tcp.source.update(updater_tcp_source, tcp_source);
				if (result != eResult::success)
				{
					YANET_LOG_ERROR("acl.transport_layer.tcp.source.update(): %s\n", result_to_c_str(result));
					return result;
				}

				flat<uint16_t>::updater updater_tcp_destination; ///< @todo
				result = transport_layer.tcp.destination.update(updater_tcp_destination, tcp_destination);
				if (result != eResult::success)
				{
					YANET_LOG_ERROR("acl.transport_layer.tcp.destination.update(): %s\n", result_to_c_str(result));
					return result;
				}

				flat<uint8_t>::updater updater_tcp_flags; ///< @todo
				result = transport_layer.tcp.flags.update(updater_tcp_flags, tcp_flags);
				if (result != eResult::success)
				{
					YANET_LOG_ERROR("acl.transport_layer.tcp.flags.update(): %s\n", result_to_c_str(result));
					return result;
				}

				flat<uint16_t>::updater updater_udp_source; ///< @todo
				result = transport_layer.udp.source.update(updater_udp_source, udp_source);
				if (result != eResult::success)
				{
					YANET_LOG_ERROR("acl.transport_layer.udp.source.update(): %s\n", result_to_c_str(result));
					return result;
				}

				flat<uint16_t>::updater updater_udp_destination; ///< @todo
				result = transport_layer.udp.destination.update(updater_udp_destination, udp_destination);
				if (result != eResult::success)
				{
					YANET_LOG_ERROR("acl.transport_layer.udp.destination.update(): %s\n", result_to_c_str(result));
					return result;
				}

				flat<uint16_t>::updater updater_icmp_type_code; ///< @todo
				result = transport_layer.icmp.type_code.update(updater_icmp_type_code, icmp_type_code);
				if (result != eResult::success)
				{
					YANET_LOG_ERROR("acl.transport_layer.icmp.type_code.update(): %s\n", result_to_c_str(result));
					return result;
				}

				flat<uint16_t>::updater updater_icmp_identifier; ///< @todo
				result = transport_layer.icmp.identifier.update(updater_icmp_identifier, icmp_identifier);
				if (result != eResult::success)
				{
					YANET_LOG_ERROR("acl.transport_layer.icmp.identifier.update(): %s\n", result_to_c_str(result));
					return result;
				}
			}
		}

		result = base.transport_table.update(request_transport_table);
		if (result != eResult::success)
		{
			return result;
		}

		result = base.total_table.update(request_total_table);
		if (result != eResult::success)
		{
			return result;
		}

		/// values
		{
			result = base.values.create(request_values.size());
			if (result != eResult::success)
			{
				return result;
			}

			std::copy(request_values.begin(), request_values.end(), base.values.pointer);
		}
	}

	generations.switch_generation_without_gc();
	return result;
}
