#pragma once

#include "common/icontrolplane.h"
#include "common/idataplane.h"

#include "helper.h"

namespace neighbor
{

void show()
{
	/// XXX: move info to dataplane
	interface::controlPlane controlplane;
	auto config = controlplane.route_config();

	interface::dataPlane dataplane;
	const auto response = dataplane.neighbor_show({});

	table_t table;
	table.insert("route_name",
	             "interface_name",
	             "ip_address",
	             "mac_address");

	/// XXX: delete find. move to dataplane
	std::map<tInterfaceId, std::string> route_names;
	std::map<tInterfaceId, std::string> interface_names;
	for (const auto& [config_route_name, config_route] : config)
	{
		for (const auto& [config_interface_name, config_interface] : config_route.interfaces)
		{
			route_names[config_interface.interfaceId] = config_route_name;
			interface_names[config_interface.interfaceId] = config_interface_name;
		}
	}

	for (const auto& [interface_id,
	                  ip_address,
	                  mac_address] : response)
	{
		table.insert(route_names[interface_id],
		             interface_names[interface_id],
		             ip_address,
		             mac_address);
	}

	table.print();
}

void insert(const std::string& route_name,
            const std::string& interface_name,
            const common::ip_address_t& ip_address,
            const common::mac_address_t& mac_address)
{
	/// XXX: move info to dataplane
	interface::controlPlane controlplane;
	auto config = controlplane.route_config();

	/// XXX: delete find. move to dataplane
	for (const auto& [config_route_name, config_route] : config)
	{
		if (config_route_name == route_name)
		{
			for (const auto& [config_interface_name, config_interface] : config_route.interfaces)
			{
				if (config_interface_name == interface_name)
				{
					interface::dataPlane dataplane;
					dataplane.neighbor_insert({config_interface.interfaceId,
					                           ip_address,
					                           mac_address});
					return;
				}
			}
		}
	}

	throw std::string("unknown route_name or interface_name");
}

}
