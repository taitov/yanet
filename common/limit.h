#pragma once

#include <optional>
#include <string>

#include "type.h"

namespace common::limit
{
using item = std::tuple<std::string, ///< name
                        std::optional<tSocketId>,
                        uint64_t, ///< current
                        uint64_t>; ///< maximum

using limits = std::vector<item>;
}
