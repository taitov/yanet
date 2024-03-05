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
	void limits(common::idp::limits::response& response);
	eResult memory_manager_update(const common::idp::memory_manager_update::request& request);
	common::idp::memory_manager_stats::response memory_manager_stats();

	void* alloc(
	        const char* name,
	        const tSocketId socket_id,
	        const uint64_t size,
	        const std::function<void(void*)>& destructor = [](void*) {});

	template<typename type,
	         typename... args_t>
	type* create(const char* name,
	             const tSocketId socket_id,
	             const uint64_t size,
	             const args_t&... args)
	{
		void* pointer = alloc(name, socket_id, size, [](void* pointer) {
			reinterpret_cast<type*>(pointer)->~type();
		});

		if (pointer == nullptr)
		{
			return nullptr;
		}

		return new (reinterpret_cast<type*>(pointer)) type(args...);
	}

	template<typename type,
	         typename... args_t>
	type* create_static_array(const char* name,
	                          const uint64_t count,
	                          const tSocketId socket_id,
	                          const args_t&... args)
	{
		void* pointer = alloc(name, socket_id, count * sizeof(type), [count](void* pointer) {
			for (uint64_t i = 0;
			     i < count;
			     i++)
			{
				type* object = (reinterpret_cast<type*>(pointer)) + i;
				object->~type();
			}
		});

		if (pointer == nullptr)
		{
			return nullptr;
		}

		for (uint64_t i = 0;
		     i < count;
		     i++)
		{
			new ((reinterpret_cast<type*>(pointer)) + i) type(args...);
		}

		return reinterpret_cast<type*>(pointer);
	}

	void destroy(void* pointer);
	void debug(tSocketId socket_id);
	bool check_memory_limit(const std::string& name, const uint64_t size);

	template<typename type>
	std::optional<uint64_t> get_available_count_static_array(const char* name,
	                                                         const tSocketId socket_id,
	                                                         const common::memory_manager::memory_group& root_memory_group)
	{
		std::map<std::string, ///< object_name
		         common::uint64> ///< current
		        currents;

		for (const auto& [pointer, memory_pointer] : pointers)
		{
			(void)pointer;

			uint64_t object_size = memory_pointer.size;
			if (memory_pointer.name == name)
			{
				object_size = 0;
			}

			currents[memory_pointer.name] = std::max(currents[memory_pointer.name].value,
			                                         object_size);
		}

		std::optional<uint64_t> result;
		root_memory_group.for_each([&](const auto& memory_group,
		                               const std::set<std::string>& object_names) {
			bool check = false;
			uint64_t group_total = 0;
			for (const auto& object_name : object_names)
			{
				group_total += currents[object_name];

				if (object_name == name)
				{
					check = true;
				}
			}

			if (check && memory_group.limit)
			{
				if (group_total <= memory_group.limit)
				{
					uint64_t count = (memory_group.limit - group_total) / sizeof(type);
					if ((!result) || count < result)
					{
						result = count;
					}
				}
			}
		});

		return result;
	}

	template<typename type>
	std::optional<uint64_t> get_available_count_static_array(const char* name,
	                                                         const tSocketId socket_id)
	{
		return get_available_count_static_array<type>(name, socket_id, root_memory_group);
	}

protected:
	cDataPlane* dataplane;

	std::mutex mutex;
	std::map<void*, memory_pointer> pointers;
	common::memory_manager::memory_group root_memory_group;
};

}
