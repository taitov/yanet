#pragma once

#include <optional>
#include <tuple>
#include <variant>

#include <inttypes.h>

#include "result.h"
#include "type.h"

namespace common::idp::neighbor
{

enum class request_type : uint32_t
{
	get,
	update,
	interfaces,
	clear,
	insert,
	remove,
	enum_size
};

//

enum class request_update_type : uint32_t
{
	interfaces,
	clear,
	insert,
	remove,
	enum_size
};

//

namespace get
{

using request = std::tuple<std::optional<std::string>, ///< route_name
                           std::optional<std::string>, ///< interface_name
                           std::optional<ip_address_t>>; ///< address

using response = std::vector<std::tuple<std::string, ///< route_name
                                        std::string, ///< interface_name
                                        ip_address_t, ///< address
                                        mac_address_t>>; ///< mac_address

}

//

namespace insert
{

using request = std::tuple<tInterfaceId, ///< interface_id
                           ip_address_t, ///< address
                           mac_address_t>; ///< mac_address

using request2 = std::tuple<std::string, ///< route_name
                            std::string, ///< interface_name
                            ip_address_t, ///< address
                            mac_address_t>; ///< mac_address

}

//

namespace remove
{

using request = std::tuple<tInterfaceId, ///< interface_id
                           ip_address_t>; ///< address

using request2 = std::tuple<std::string, ///< route_name
                            std::string, ///< interface_name
                            ip_address_t>; ///< address

}

//

namespace update
{

using request = std::vector<std::tuple<request_type,
                                       std::variant<insert::request,
                                                    remove::request>>>;

using response = eResult;

}

//

using request = std::tuple<request_type,
                           std::variant<std::tuple<>,
                                        insert::request>>;

using response = std::variant<eResult>;

}
