#pragma once

#include <rte_ether.h>

#include "common/type.h"

#include "common.h"
#include "hashtable.h"
#include "type.h"

namespace dataplane
{

struct neighbor_v4_key
{
	tInterfaceId interface_id;
	ipv4_address_t address;
};

struct neighbor_v6_key
{
	tInterfaceId interface_id;
	ipv6_address_t address;
};

struct neighbor_value
{
	rte_ether_addr ether_address;
};

class neighbor
{
public:
	hashtable_mod_spinlock_dynamic<neighbor_v4_key, neighbor_value, 16>* neighbor_v4_ht;
	hashtable_mod_spinlock_dynamic<neighbor_v6_key, neighbor_value, 16>* neighbor_v6_ht;
};

}
