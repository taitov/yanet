#include "acl.h"
#include "dataplane.h"
#include "worker.h"

using namespace dataplane::acl;

module::module() :dataplane(nullptr)
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
	generations.switch_generation_without_gc();

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

eResult module::acl_update(const common::acl::idp::request& request)
{
	const auto& [request_transport_table, request_total_table] = request;

	eResult result = eResult::success;

	{
		auto lock = generations.next_lock_guard();
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
	}

	return result;
}

void module::acl_flush()
{
	generations.next_lock();
	generations.switch_generation_without_gc();

	auto& generation = generations.next();
	for (const auto socket_id : dataplane->get_socket_ids())
	{
		auto& base = generation.bases.find(socket_id)->second;

		base.transport_table_updater.free();
		base.total_table_updater.free();
	}

	generations.next_unlock();
}
