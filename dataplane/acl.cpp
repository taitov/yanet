#include "acl.h"
#include "dataplane.h"
#include "worker.h"

using namespace dataplane::acl;

base::base(memory_manager* memory_manager,
           const tSocketId socket_id) :
        network_ipv4_source("acl.network.v4.source.lpm", memory_manager, socket_id),
        network_ipv4_destination("acl.network.v4.destination.lpm", memory_manager, socket_id),
        transport_table("acl.transport.ht", memory_manager, socket_id),
        total_table("acl.total.ht", memory_manager, socket_id)
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
		worker_base->acl_transport_table = base.transport_table.pointer;
		worker_base->acl_total_table = base.total_table.pointer;
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
		base.transport_table.report(json["acl"]["transport_table"]);
		base.total_table.report(json["acl"]["total_table"]);
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
		base.transport_table.limits(response);
		base.total_table.limits(response);
	}
}

inline const auto& get_request(const common::idp::update::request& request)
{
	const auto& [request_globalbase, request_acl, request_neighbor] = request;
	(void)request_globalbase;
	(void)request_neighbor;

	return request_acl;
}

inline auto& get_response(common::idp::update::response& response)
{
	auto& [response_globalbase, response_acl, response_neighbor] = response;
	(void)response_globalbase;
	(void)response_neighbor;

	return response_acl;
}

void module::update_before(const common::idp::update::request& request)
{
	const auto& request_acl = get_request(request);
	if (!request_acl)
	{
		return;
	}

	generations.next_lock();
}

void module::update(const common::idp::update::request& request,
                    common::idp::update::response& response)
{
	const auto& request_acl = get_request(request);
	if (!request_acl)
	{
		return;
	}

	auto& response_acl = get_response(response);
	response_acl = acl_update(*request_acl);

	return;
}

void module::update_after(const common::idp::update::request& request)
{
	const auto& request_acl = get_request(request);
	if (!request_acl)
	{
		return;
	}

	auto& generation = generations.next();
	for (const auto socket_id : dataplane->get_socket_ids())
	{
		auto& base = generation.bases.find(socket_id)->second;

		base.network_ipv4_source.free();
		base.network_ipv4_destination.free();
		base.transport_table.clear();
		base.total_table.clear();
	}

	generations.next_unlock();
}

eResult module::acl_update(const common::acl::idp::request& request_acl)
{
	eResult result = eResult::success;
	const auto& [request_network_ipv4_source,
	             request_network_ipv4_destination,
	             request_transport_table,
	             request_total_table] = request_acl;

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
	}

	generations.switch_generation_without_gc();
	return result;
}
