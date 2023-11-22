#include "neighbor.h"
#include "controlplane.h"
#include "dataplane.h"

using namespace dataplane::neighbor;

neighbor::neighbor()
{
}

void neighbor::insert(const tInterfaceId interface_id,
                      const common::ip_address_t address,
                      const common::mac_address_t& mac_address)
{
	std::lock_guard<std::mutex> requests_lock(requests_mutex);
	requests.emplace_back(common::idp::update_neighbor::request_type::insert,
	                      common::idp::update_neighbor::insert::request(interface_id, address, mac_address));
}

void neighbor::thread()
{
	generations.next_lock();

	generations.switch_generation_without_clear();
	generations.next_unlock();
}

void neighbor::monitor_thread()
{
	insert(1, {"200.0.0.1"}, {"00:11:22:33:44:55"});
}

void generation::insert(const common::idp::update_neighbor::insert::request& request)
{
	const auto& [interface_id, address, mac_address] = request;

	value value;
	memcpy(value.ether_address.addr_bytes, mac_address.data(), 6);

	if (address.is_ipv4())
	{
		key_v4 key;
		key.interface_id = interface_id;
		key.address = ipv4_address_t::convert(address.get_ipv4());

		if (!ht_v4->insert_or_update(key, value))
		{
			/// XXX
		}
	}
	else
	{
		key_v6 key;
		key.interface_id = interface_id;
		key.address = ipv6_address_t::convert(address.get_ipv6());

		if (!ht_v6->insert_or_update(key, value))
		{
			/// XXX
		}
	}
}
