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
		generation.transport_table_updater.init("acl.transport.ht", &dataplane->memory_manager);
	});

	/// initial allocate objects
	for (const auto socket_id : dataplane->get_socket_ids())
	{
		auto& generation = generations.next();

		{
			common::idp::acl_transport_table::request request;
			result = generation.transport_table_updater.update(socket_id, request);
			if (result != eResult::success)
			{
				return result;
			}
		}
	}
	generations.switch_generation_without_gc();

	threads.emplace_back([this]() {
		main_thread();
	});

	return result;
}

void module::update_worker_base(const std::vector<std::tuple<tSocketId, dataplane::base::generation*>>& base_nexts)
{
	auto lock = generations.current_lock_guard();

	const auto& generation = generations.current();
	for (auto& [socket_id, base_next] : base_nexts)
	{
		base_next->acl_transport_table = generation.transport_table_updater.get_pointer(socket_id);
	}
}

void module::report(nlohmann::json& json)
{
	/// XXX
	(void)json;
}

void module::limits(common::idp::limits::response& response)
{
	/// XXX
	(void)response;
}

eResult module::acl_update(const common::acl::idp::request& request)
{
	const auto& [request_transport_table] = request;

	eResult result = eResult::success;

	result = acl_transport_table(request_transport_table);
	if (result != eResult::success)
	{
		return result;
	}

	return result;
}

eResult module::acl_transport_table(const common::idp::acl_transport_table::request& request)
{
	auto lock = generations.next_lock_guard();

	auto& generation = generations.next();
	for (const auto socket_id : dataplane->get_socket_ids())
	{
		auto result = generation.transport_table_updater.update(socket_id, request);
		if (result != eResult::success)
		{
			return result;
		}
	}

	return eResult::success;
}

void module::acl_flush()
{
	generations.next_lock();
	generations.switch_generation_without_gc();
	generations.next().transport_table_updater.clear();
	generations.next_unlock();
}

void module::main_thread()
{
	for (;;)
	{
		//		generations.switch_generation_with_update([this]() {
		//			dataplane->switch_worker_base();
		//		});

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}
