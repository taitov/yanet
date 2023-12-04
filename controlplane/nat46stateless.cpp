#include "nat46stateless.h"
#include "controlplane.h"

eResult nat46stateless_t::init()
{
	controlPlane->register_counter(module_counters);

	//	controlPlane->register_command(common::icp::requestType::nat64stateful_config, [this]() {
	//		return nat64stateful_config();
	//	});

	//	controlPlane->register_command(common::icp::requestType::nat64stateful_announce, [this]() {
	//		return nat64stateful_announce();
	//	});

	funcThreads.emplace_back([this]() {
		counters_gc_thread();
	});

	return eResult::success;
}

void nat46stateless_t::reload_before()
{
	generations_config.next_lock();
}

void nat46stateless_t::reload(const controlplane::base_t& base_prev,
                              const controlplane::base_t& base_next,
                              common::idp::updateGlobalBase::request& globalbase)
{
	generations_config.next().update(base_prev, base_next);

	for (const auto& [name, nat46stateless] : base_next.nat46statelesses)
	{
		(void)nat46stateless;

		module_counters.insert(name);
	}

	for (const auto& [name, nat46stateless] : base_prev.nat46statelesses)
	{
		(void)nat46stateless;

		module_counters.remove(name);
	}

	module_counters.allocate();

	compile(globalbase, generations_config.next());
}

void nat46stateless_t::reload_after()
{
	module_counters.release();
	generations_config.switch_generation();
	generations_config.next_unlock();
}

// common::icp::nat64stateful_config::response nat64stateful_t::nat64stateful_config() const
//{
//	auto config_current_guard = generations_config.current_lock_guard();
//	return generations_config.current().config_nat64statefuls;
// }

// common::icp::nat64stateful_announce::response nat64stateful_t::nat64stateful_announce() const
//{
//	auto current_guard = generations_config.current_lock_guard();
//	return generations_config.current().announces;
// }

void nat46stateless_t::compile(common::idp::updateGlobalBase::request& globalbase,
                               nat46stateless::generation_config_t& generation_config)
{
	(void)globalbase;
	(void)generation_config;
	//	globalbase.emplace_back(common::idp::updateGlobalBase::requestType::nat64stateful_pool_update,
	//	                        std::move(pool));
}
