#pragma once

#include "base.h"
#include "counter.h"
#include "module.h"

#include "common/controlplaneconfig.h"
#include "common/counters.h"
#include "common/generation.h"
#include "common/icp.h"
#include "common/idataplane.h"

namespace nat46stateless
{

using module_counter_key_t = std::string; ///< module_name

class generation_config_t
{
public:
	void update(const controlplane::base_t& base_prev,
	            const controlplane::base_t& base_next)
	{
		(void)base_prev;

		config_nat46statelesses = base_next.nat46statelesses;

		//		for (const auto& [name, nat64stateful] : base_next.nat64statefuls)
		//		{
		//			for (const auto& prefix : nat64stateful.announces)
		//			{
		//				announces.emplace(name, prefix);
		//			}
		//		}
	}

public:
	std::map<std::string, controlplane::nat46stateless::config_t> config_nat46statelesses;
	common::icp::nat46stateless_announce::response announces;
};

}

class nat46stateless_t : public module_t
{
public:
	eResult init() override;
	void reload_before() override;
	void reload(const controlplane::base_t& base_prev, const controlplane::base_t& base_next, common::idp::updateGlobalBase::request& globalbase) override;
	void reload_after() override;

//	common::icp::nat64stateful_config::response nat64stateful_config() const;
//	common::icp::nat64stateful_announce::response nat64stateful_announce() const;

	void compile(common::idp::updateGlobalBase::request& globalbase, nat46stateless::generation_config_t& generation_config);

protected:
	void counters_gc_thread();

protected:
	interface::dataPlane dataplane;
	generation_manager<nat46stateless::generation_config_t> generations_config;
	counter_t<nat46stateless::module_counter_key_t, (size_t)nat46stateless::module_counter::size> module_counters;
};
