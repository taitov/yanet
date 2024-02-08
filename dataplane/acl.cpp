#include "acl.h"
#include "dataplane.h"
#include "worker.h"

using namespace dataplane::acl;

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
			auto& base = generation.bases[socket_id];

			base.transport_table_updater.init("acl.transport.ht", &dataplane->memory_manager, socket_id);
			base.total_table_updater.init("acl.total.ht", &dataplane->memory_manager, socket_id);
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

		worker_base->acl_transport_table = base.transport_table_updater.get_pointer();
		worker_base->acl_total_table = base.total_table_updater.get_pointer();
	}
}

void module::report(nlohmann::json& json)
{
	/// XXX
	(void)json;
}

void module::limits(common::idp::limits::response& response)
{
	auto lock = generations.current_lock_guard();

	const auto& generation = generations.current();
	for (const auto socket_id : dataplane->get_socket_ids())
	{
		const auto& base = generation.bases.find(socket_id)->second;

		base.transport_table_updater.limits(response);
		base.total_table_updater.limits(response);
	}
}

inline const std::optional<common::acl::idp::request>& get_request(const common::idp::update::request& request)
{
	const auto& [request_globalbase, request_acl] = request;
	(void)request_globalbase;

	return request_acl;
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

eResult module::update(const common::idp::update::request& request)
{
	eResult result = eResult::success;

	const auto& request_acl = get_request(request);
	if (!request_acl)
	{
		return result;
	}

	return acl_update(*request_acl);
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

		base.transport_table_updater.free();
		base.total_table_updater.free();
	}

	generations.next_unlock();
}

eResult module::acl_update(const common::acl::idp::request& request_acl)
{
	eResult result = eResult::success;
	const auto& [request_transport_table, request_total_table] = request_acl;

	auto& generation = generations.next();
	for (const auto socket_id : dataplane->get_socket_ids())
	{
		auto& base = generation.bases.find(socket_id)->second;

		result = base.transport_table_updater.update(request_transport_table);
		if (result != eResult::success)
		{
			return result;
		}

		result = base.total_table_updater.update(request_total_table);
		if (result != eResult::success)
		{
			return result;
		}
	}

	generations.switch_generation_without_gc();
	return result;
}
