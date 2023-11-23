#pragma once

#include <tuple>
#include <variant>

#include <inttypes.h>

#include "result.h"
#include "type.h"

namespace common::idp::neighbor
{

enum class request_type : uint32_t
{
	clear,
	get,
	insert,
	remove,
	interfaces,
	size
};

namespace insert
{

using request = std::tuple<tInterfaceId, ///< interface_id
                           ip_address_t, ///< address
                           mac_address_t>; ///< mac_address

}

using request = std::tuple<request_type,
                           std::variant<std::tuple<>,
                                        insert::request>>;

using response = std::variant<eResult>;

}
