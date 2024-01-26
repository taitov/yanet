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

class generation
{
public:
	acl::transport_table transport_table_updater;
};

//

class module
{
public:
	module();

public:
	eResult init(cDataPlane* dataplane);
	void update_worker_base(const std::vector<std::tuple<tSocketId, dataplane::base::generation*>>& base_nexts);
	void report(nlohmann::json& json);
	void limits(common::idp::limits::response& response);

	eResult acl_update(const common::acl::idp::request& request);
	eResult acl_transport_table(const common::idp::acl_transport_table::request& request);
	void acl_flush();

protected:
	void main_thread();

protected:
	cDataPlane* dataplane;
	std::vector<std::thread> threads;
	generation_manager<dataplane::acl::generation> generations;
};

}
