#pragma once

#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "common/generation.h"
#include "common/idp.h"

#include "type.h"

namespace dataplane
{

class module
{
public:
	module();

public:
	eResult init(cDataPlane* dataplane);
	void report(nlohmann::json& json);
	void limits(common::idp::limits::response& response);

protected:
	cDataPlane* dataplane;
};

}
