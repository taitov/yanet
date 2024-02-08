#pragma once

#include <thread>

#include <nlohmann/json.hpp>

#include "common/acl.h"
#include "common/generation.h"
#include "common/idp.h"

#include "type.h"
#include "updater.h"

namespace dataplane::acl
{

using transport_table = dataplane::updater_hashtable_mod_id32<common::acl::transport_key_t, 16>;
using total_table = dataplane::updater_hashtable_mod_id32<common::acl::total_key_t, 16>;

class base
{
public:
	acl::transport_table transport_table_updater;
	acl::total_table total_table_updater;
};

class generation
{
public:
	std::map<tSocketId, base> bases;
};

//

class module
{
public:
	module();

public:
	eResult init(cDataPlane* dataplane);
	void update_worker_base(const std::vector<std::tuple<tSocketId, dataplane::base::generation*>>& worker_base_nexts);
	void report(nlohmann::json& json);
	void limits(common::idp::limits::response& response);

	void update_before(const common::idp::update::request& request);
	eResult update(const common::idp::update::request& request);
	void update_after(const common::idp::update::request& request);

protected:
	eResult acl_update(const common::acl::idp::request& request_acl);

protected:
	cDataPlane* dataplane;
	std::vector<std::thread> threads;
	generation_manager<dataplane::acl::generation> generations;
};

}
