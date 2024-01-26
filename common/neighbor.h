#pragma once

#include <inttypes.h>
#include <tuple>
#include <variant>
#include <vector>

#include "result.h"
#include "type.h"

namespace common::neighbor
{

struct stats
{
	uint64_t hashtable_insert_success;
	uint64_t hashtable_insert_error;
	uint64_t hashtable_remove_success;
	uint64_t hashtable_remove_error;
	uint64_t netlink_neighbor_update;
	uint64_t resolve;
};

//

namespace idp
{

enum class type
{
	show,
	insert,
	remove,
	clear,
	update_interfaces,
	stats
};

using show = std::vector<std::tuple<std::string, ///< route_name
                                    std::string, ///< interface_name
                                    ip_address_t, ///< ip_address
                                    mac_address_t, ///< mac_address
                                    std::optional<uint32_t>>>; ///< last_update_timestamp

using insert = std::tuple<std::string, ///< route_name
                          std::string, ///< interface_name
                          ip_address_t, ///< ip_address
                          mac_address_t>; ///< mac_address

using remove = std::tuple<std::string, ///< route_name
                          std::string, ///< interface_name
                          ip_address_t>; ///< ip_address

using update_interfaces = std::vector<std::tuple<tInterfaceId, ///< interface_id
                                                 std::string, ///< route_name
                                                 std::string>>; ///< interface_name

using stats = common::neighbor::stats;

using request = std::tuple<type,
                           std::variant<std::tuple<>,
                                        insert,
                                        remove,
                                        update_interfaces>>;

using response = std::variant<eResult,
                              show,
                              stats>;

}

}
