#pragma once

#include <arpa/inet.h>

#include <rte_common.h>
#include <rte_ether.h>
#include <rte_gre.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include "acl.h"
#include "neighbor.h"
#include "type.h"

namespace dataplane::base
{

class permanently
{
public:
	permanently() :
	        globalBaseAtomic(nullptr),
	        workerPortsCount(0),
	        ports_count(0),
	        nat64stateful_numa_mask(0xFFFFu),
	        nat64stateful_numa_reverse_mask(0),
	        nat64stateful_numa_id(0)
	{
		memset(globalBaseAtomics, 0, sizeof(globalBaseAtomics));

		memset(transportSizes, 0, sizeof(transportSizes));

		transportSizes[IPPROTO_TCP] = sizeof(rte_tcp_hdr);
		transportSizes[IPPROTO_UDP] = sizeof(rte_udp_hdr);
		transportSizes[IPPROTO_ICMP] = sizeof(icmp_header_t);
		transportSizes[IPPROTO_ICMPV6] = sizeof(icmpv6_header_t);
		transportSizes[IPPROTO_GRE] = sizeof(rte_gre_hdr);
	}

	bool add_worker_port(const tPortId port_id,
	                     tQueueId queue_id)
	{
		if (workerPortsCount >= CONFIG_YADECAP_WORKER_PORTS_SIZE)
		{
			return false;
		}

		workerPorts[workerPortsCount].inPortId = port_id;
		workerPorts[workerPortsCount].inQueueId = queue_id;
		workerPortsCount++;

		return true;
	}

	dataplane::globalBase::atomic* globalBaseAtomic;
	/// Pointers to all globalBaseAtomic for each CPU socket.
	///
	/// Used to distribute firewall states.
	dataplane::globalBase::atomic* globalBaseAtomics[YANET_CONFIG_NUMA_SIZE];

	unsigned int workerPortsCount;
	struct
	{
		tPortId inPortId;
		tQueueId inQueueId;
	} workerPorts[CONFIG_YADECAP_WORKER_PORTS_SIZE];

	tPortId ports[CONFIG_YADECAP_PORTS_SIZE];
	unsigned int ports_count;
	tQueueId outQueueId;

	uint32_t SWNormalPriorityRateLimitPerWorker;
	uint8_t transportSizes[256];

	uint16_t nat64stateful_numa_mask;
	uint16_t nat64stateful_numa_reverse_mask;
	uint16_t nat64stateful_numa_id;
};

class generation
{
public:
	generation() :
	        globalBase(nullptr),
	        neighbor_hashtable(nullptr),
	        acl_transport_table(nullptr),
	        acl_total_table(nullptr)
	{
	}

	dataplane::globalBase::generation* globalBase;
	const dataplane::neighbor::hashtable* neighbor_hashtable;
	const dataplane::acl::network_ipv4_source::object_type* acl_network_ipv4_source;
	const dataplane::acl::network_ipv4_destination::object_type* acl_network_ipv4_destination;
	const dataplane::acl::network_ipv6_destination_ht::object_type* acl_network_ipv6_destination_ht;
	const dataplane::acl::transport_table::object_type* acl_transport_table;
	const dataplane::acl::total_table::object_type* acl_total_table;
} __rte_aligned(2 * RTE_CACHE_LINE_SIZE);

}
