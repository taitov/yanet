#pragma once

#include <mutex>

#include <rte_malloc.h>

#include "common/idp.h"
#include "common/memory_manager.h"
#include "common/result.h"
#include "type.h"

namespace dataplane
{

class memory_pointer
{
public:
	memory_pointer(const char* name, const tSocketId socket_id, const size_t size, void* pointer, const std::function<void(void*)>& destructor);
	~memory_pointer();

public:
	std::string name;
	tSocketId socket_id;
	size_t size;
	void* pointer;
	std::function<void(void*)> destructor;
};

class memory_manager
{
public:
	memory_manager();

	eResult init(cDataPlane* dataplane);
	void report(nlohmann::json& json);
	eResult memory_manager_update(const common::idp::memory_manager_update::request& request);

	void* alloc(const char* name, const tSocketId socket_id, const uint64_t size);
	void free(void* pointer);
	void debug(tSocketId socket_id);

	//	template<typename type,
	//	         typename... args_t>
	//	type* create_static(const char* name,
	//	                    int socket_id,
	//	                    const args_t&... args)
	//	{
	//		size_t size = sizeof(type) + 2 * RTE_CACHE_LINE_SIZE;

	//		YANET_LOG_INFO("yanet_alloc(name: '%s', size: %lu, socket: %u)\n",
	//		               name,
	//		               size,
	//		               socket_id);

	//		void* pointer = rte_zmalloc_socket(nullptr,
	//		                                   size,
	//		                                   RTE_CACHE_LINE_SIZE,
	//		                                   socket_id);
	//		if (pointer == nullptr)
	//		{
	//			YANET_LOG_ERROR("error allocation memory for '%s' with size: '%lu'\n",
	//			                name,
	//			                size);
	//			debug(socket_id);
	//			return nullptr;
	//		}

	//		type* result = new ((type*)pointer) type(args...);

	//		{
	//			std::lock_guard<std::mutex> guard(pointers_mutex);
	//			pointers.try_emplace(result, name, size, [result]() {
	//				YANET_LOG_INFO("yanet_free()\n");
	//				result->~type();
	//				rte_free(result);
	//			});
	//		}

	//		return result;
	//	}

	//	template<typename type,
	//	         typename elems_t,
	//	         typename updater_type,
	//	         typename... args_t>
	//	type* create_dynamic(const char* name,
	//	                     int socket_id,
	//	                     elems_t elems,
	//	                     updater_type& updater,
	//	                     const args_t&... args)
	//	{
	//		size_t size = type::calculate_sizeof(elems);
	//		if (!size)
	//		{
	//			return nullptr;
	//		}

	//		size += 2 * RTE_CACHE_LINE_SIZE;

	//		YANET_LOG_INFO("yanet_alloc(name: '%s', size: %lu, socket: %u)\n",
	//		               name,
	//		               size,
	//		               socket_id);

	//		void* pointer = rte_zmalloc_socket(nullptr,
	//		                                   size,
	//		                                   RTE_CACHE_LINE_SIZE,
	//		                                   socket_id);
	//		if (pointer == nullptr)
	//		{
	//			YANET_LOG_ERROR("error allocation memory for '%s' with size: '%lu'\n",
	//			                name,
	//			                size);
	//			debug(socket_id);
	//			return nullptr;
	//		}

	//		type* result = new ((type*)pointer) type(args...);

	//		{
	//			std::lock_guard<std::mutex> guard(pointers_mutex);
	//			pointers.try_emplace(result, name, size, [result]() {
	//				YANET_LOG_INFO("yanet_free()\n");
	//				result->~type();
	//				rte_free(result);
	//			});
	//		}

	//		updater.update_pointer(name, result, socket_id, elems, args...);

	//		return result;
	//	}

	//	template<typename type,
	//	         typename container_keys_type,
	//	         typename updater_type,
	//	         typename... args_t>
	//	eResult create_hashtable_dynamic(const char* name,
	//	                                 int socket_id,
	//	                                 const container_keys_type& keys,
	//	                                 updater_type& updater,
	//	                                 const args_t&... args)
	//	{
	//		eResult result = eResult::success;

	//		uint64_t size = 1;
	//		{
	//			uint64_t keys_size = std::max((size_t)128, keys.size());
	//			while (size < 4ull * keys_size)
	//			{
	//				size <<= 1;
	//			}
	//		}

	//		for (;;)
	//		{
	//			destroy(updater.get_pointer());

	//			auto* pointer = create_dynamic<type>(name, socket_id, size, updater);
	//			if (!pointer)
	//			{
	//				return eResult::errorAllocatingMemory;
	//			}

	//			result = updater.update(keys);
	//			if (result != eResult::success)
	//			{
	//				size <<= 1;
	//				continue;
	//			}

	//			printf("XXX4: create_hashtable_dynamic: %p\n", pointer);

	//			break;
	//		}

	//		return result;
	//	}

	//	void destroy(void* pointer);

protected:
	cDataPlane* dataplane;
	common::memory_manager::memory_group root_memory_group;

	std::mutex pointers_mutex;
	std::map<void*, memory_pointer> pointers;
};

}
