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
	uint16_t last_update_timestamp;
};

//

using hashtable_v4 = hashtable_mod_dynamic<key_v4, value, 16>;
using hashtable_v6 = hashtable_mod_dynamic<key_v6, value, 16>;

using interface = std::tuple<tInterfaceId, ///< interface_id
                             std::string, ///< route_name
                             std::string>; ///< interface_name

//

class socket
{
public:
	eResult init(cDataPlane* dataplane, const tSocketId socket_id);

	void insert(const common::idp::neighbor::insert::request& request);

public:
	hashtable_v4::updater updater_v4;
	hashtable_v6::updater updater_v6;
};

//

class generation
{
public:
	generation();

public:
	eResult init(cDataPlane* dataplane);

public:
	std::vector<interface> interfaces;
	std::map<tSocketId, socket> sockets;
};

//

class neighbor
{
public:
	neighbor();

public:
	eResult init(cDataPlane* dataplane);
	common::idp::neighbor::response update(const common::idp::neighbor::request& request);
	void insert(const tInterfaceId interface_id, const common::ip_address_t address, const common::mac_address_t& mac_address);

protected:
	void thread();
	void monitor_thread();

protected:
	cDataPlane* dataplane;

	std::mutex requests_mutex;
	std::vector<common::idp::neighbor::request> requests;

	generation_manager<generation> generations;
};

}
