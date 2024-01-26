#pragma once

#include <thread>

#include <nlohmann/json.hpp>

#include "common/acl.h"
#include "common/generation.h"
#include "common/idp.h"

#include "lpm.h"
#include "type.h"
#include "updater.h"

namespace dataplane::acl
{

using network_ipv4_source = dataplane::updater_lpm4_24bit_8bit_id32;
using network_ipv4_destination = dataplane::updater_lpm4_24bit_8bit_id32;
using network_ipv6_destination_ht = dataplane::updater_hashtable_mod_id32<ipv6_address_t, 1>;
using transport_table = dataplane::updater_hashtable_mod_id32<common::acl::transport_key_t, 16>;
using total_table = dataplane::updater_hashtable_mod_id32<common::acl::total_key_t, 16>;

class base
{
public:
	base(dataplane::memory_manager* memory_manager, const tSocketId socket_id);

public:
	acl::network_ipv4_source network_ipv4_source;
	acl::network_ipv4_destination network_ipv4_destination;
	acl::network_ipv6_destination_ht network_ipv6_destination_ht;
	acl::transport_table transport_table;
	acl::total_table total_table;
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
	void update(const common::idp::update::request& request, common::idp::update::response& response);
	void update_after(const common::idp::update::request& request);

protected:
	eResult acl_update(const common::acl::idp::request& request_acl);

protected:
	cDataPlane* dataplane;
	std::vector<std::thread> threads;
	generation_manager<dataplane::acl::generation> generations;
};

}
