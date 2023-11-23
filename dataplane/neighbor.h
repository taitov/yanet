#pragma once

#include <rte_ether.h>

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

using hashtable_v4 = hashtable_mod_spinlock_dynamic<key_v4, value, 16>;
using hashtable_v6 = hashtable_mod_spinlock_dynamic<key_v6, value, 16>;

using interface = std::tuple<tInterfaceId, ///< interface_id
                             std::string, ///< route_name
                             std::string>; ///< interface_name

}
