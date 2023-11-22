#pragma once

#include <rte_ether.h>

#include "common/generation.h"
#include "common/idp_neighbor.h"
#include "common/type.h"

#include "common.h"
#include "hashtable.h"
#include "type.h"

namespace dataplane::neighbor
{

struct key_v4
{
	tInterfaceId interface_id;
	ipv4_address_t address;
};

struct key_v6
{
	tInterfaceId interface_id;
	ipv6_address_t address;
};

struct value
{
	rte_ether_addr ether_address;
};

using interface = std::tuple<tInterfaceId, ///< interface_id
                             std::string, ///< route_name
                             std::string>; ///< interface_name

class generation
{
public:
	void insert(const common::idp::update_neighbor::insert::request& request);

public:
	std::vector<interface> interfaces;
	hashtable_mod_spinlock_dynamic<key_v4, value, 16>* ht_v4;
	hashtable_mod_spinlock_dynamic<key_v6, value, 16>* ht_v6;
};

class neighbor
{
public:
	neighbor();

public:
	common::idp::update_neighbor::response update(const common::idp::update_neighbor::request& requests);
	void insert(const tInterfaceId interface_id, const common::ip_address_t address, const common::mac_address_t& mac_address);

protected:
	void thread();
	void monitor_thread();

protected:
	cControlPlane* controlplane;
	cDataPlane* dataplane;

	std::mutex requests_mutex;
	std::vector<common::idp::update_neighbor::request> requests;

	generation_manager<generation> generations;
};

}
