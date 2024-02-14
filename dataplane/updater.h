#pragma once

#include <mutex>
#include <nlohmann/json.hpp>
#include <rte_malloc.h>

#include "common.h"
#include "hashtable.h"
#include "lpm.h"
#include "memory_manager.h"

namespace dataplane
{

[[maybe_unused]] static uint32_t upper_power_of_two(const uint32_t value)
{
	/// @todo: use __builtin_clz

	uint32_t result = 1;
	while (result < value)
	{
		result <<= 1;
		if (!result)
		{
			return 0;
		}
	}
	return result;
}

/// XXX: MOVE
namespace limits
{
using limit = std::tuple<std::string, ///< name
                         std::optional<tSocketId>,
                         uint64_t, ///< current
                         uint64_t>; ///< maximum

using response = std::vector<limit>;
}

class updater_base
{
public:
	updater_base();
	virtual ~updater_base();

	eResult init(const char* name, dataplane::memory_manager* memory_manager, const tSocketId socket_id);
	eResult alloc(const uint64_t memory_size);
	void* get_pointer();
	const void* get_pointer() const;
	void free();

protected:
	std::string name;
	dataplane::memory_manager* memory_manager;
	tSocketId socket_id;
	void* pointer;
};

template<typename type>
class updater
{
public:
	using object_type = type;

	updater(const char* name, dataplane::memory_manager* memory_manager, const tSocketId socket_id) :
	        name(name),
	        memory_manager(memory_manager),
	        socket_id(socket_id),
	        pointer(nullptr),
	        object_size(0)
	{
	}

	virtual ~updater()
	{
		free();
	}

	//	template<typename... args_t>
	//	object_type* create(const uint64_t size,
	//	                    const args_t&... args)
	//	{
	//		if (alloc(size) != eResult::success)
	//		{
	//			return nullptr;
	//		}

	//		object_type* result = new (get_pointer()) object_type(args...);
	//		/// XXX:destructor
	//		// {
	//		// 	result->~type();
	//		// }
	//		return result;
	//	}

	virtual void limits(limits::response& limits) const
	{
		(void)limits;
	}

	virtual void report(nlohmann::json& report) const
	{
		(void)report;
	}

	eResult allocddd(const uint64_t memory_size)
	{
		(void)memory_size;
		return eResult::success;
	}

	void free()
	{
		if (pointer)
		{
			memory_manager->free(pointer);
			pointer = nullptr;
		}
	}

protected:
	std::string name;
	dataplane::memory_manager* memory_manager;
	tSocketId socket_id;

public:
	object_type* pointer;
	uint64_t object_size;
};

class updater_lpm4_24bit_8bit_id32
{
public:
	using object_type = lpm4_24bit_8bit_id32_dynamic;

	updater_lpm4_24bit_8bit_id32(const char* name,
	                             dataplane::memory_manager* memory_manager,
	                             const tSocketId socket_id) :
	        name(name),
	        memory_manager(memory_manager),
	        socket_id(socket_id),
	        extended_chunks_size(1),
	        extended_chunks_count(0),
	        pointer(nullptr)
	{
	}

	static uint64_t calculate_sizeof(const uint64_t extended_chunks_size)
	{
		if (!extended_chunks_size)
		{
			YANET_LOG_ERROR("wrong extended_chunks_size: %lu\n", extended_chunks_size);
			return 0;
		}

		return sizeof(object_type) + extended_chunks_size * sizeof(object_type::chunk_step2_t);
	}

	eResult update(const std::vector<common::acl::tree_chunk_8bit_t>& values)
	{
		extended_chunks_size = std::max((uint64_t)8, values.size() / 16);

		for (;;)
		{
			pointer = memory_manager->create<object_type>(name.data(),
			                                              socket_id,
			                                              calculate_sizeof(extended_chunks_size));
			if (pointer == nullptr)
			{
				return eResult::errorAllocatingMemory;
			}

			eResult result = fill(values);
			if (result != eResult::success)
			{
				/// try again

				extended_chunks_size <<= 1;
				continue;
			}

			break;
		}

		return eResult::success;
	}

	void clear()
	{
		if (pointer)
		{
			memory_manager->free(pointer);
			pointer = nullptr;
		}
	}

	void limits(limits::response& limits) const
	{
		limits.emplace_back(name + ".extended_chunks",
		                    socket_id,
		                    extended_chunks_count,
		                    extended_chunks_size);
	}

	void report(nlohmann::json& report) const
	{
		report["extended_chunks_count"] = extended_chunks_count;
	}

protected:
	inline unsigned int allocate_extended_chunk()
	{
		if (extended_chunks_count >= extended_chunks_size)
		{
			return 0;
		}

		unsigned int new_chunk_id = extended_chunks_count;
		extended_chunks_count++;

		return new_chunk_id;
	}

	eResult fill(const std::vector<common::acl::tree_chunk_8bit_t>& values)
	{
		extended_chunks_count = 1;
		remap_chunks.resize(0);
		remap_chunks.resize(values.size(), 0);

		if (values.empty())
		{
			return eResult::success;
		}

		return update_root_chunk<0>(values, 0, 0);
	}

	template<unsigned int bits_offset>
	eResult update_root_chunk(const std::vector<common::acl::tree_chunk_8bit_t>& from_chunks,
	                          const unsigned int from_chunk_id,
	                          const unsigned int root_chunk_values_offset)
	{
		eResult result = eResult::success;

		const auto& from_chunk = from_chunks[from_chunk_id];
		for (uint32_t i = 0;
		     i < (1u << 8);
		     i++)
		{
			const unsigned int root_chunk_values_i = root_chunk_values_offset + (i << (object_type::bits_step1 - bits_offset - 8));
			const auto& from_chunk_value = from_chunk.values[i];

			if (from_chunk_value.is_chunk_id())
			{
				if constexpr (bits_offset < object_type::bits_step1 - object_type::bits_step2)
				{
					result = update_root_chunk<bits_offset + 8>(from_chunks,
					                                            from_chunk_value.get_chunk_id(),
					                                            root_chunk_values_i);
					if (result != eResult::success)
					{
						return result;
					}
				}
				else if constexpr (bits_offset < object_type::bits_step1)
				{
					auto& root_chunk_value = pointer->root_chunk.values[root_chunk_values_i];

					auto& extended_chunk_id = remap_chunks[from_chunk_value.get_chunk_id()];
					if (!extended_chunk_id)
					{
						extended_chunk_id = allocate_extended_chunk();
						if (!extended_chunk_id)
						{
							YANET_LOG_ERROR("lpm is full\n");
							return eResult::isFull;
						}

						result = update_extended_chunk(from_chunks,
						                               from_chunk_value.get_chunk_id(),
						                               extended_chunk_id);
						if (result != eResult::success)
						{
							return result;
						}
					}

					root_chunk_value.id = extended_chunk_id ^ 0x80000000u;
				}
				else
				{
					YANET_LOG_ERROR("tree broken\n");
					return eResult::invalidArguments;
				}
			}
			else
			{
				if constexpr (bits_offset < object_type::bits_step1)
				{
					for (uint32_t j = 0;
					     j < (1u << (object_type::bits_step1 - bits_offset - 8));
					     j++)
					{
						pointer->root_chunk.values[root_chunk_values_i + j].id = from_chunk_value.get_group_id();
					}
				}
				else
				{
					YANET_LOG_ERROR("tree broken\n");
					return eResult::invalidArguments;
				}
			}
		}

		return result;
	}

	eResult update_extended_chunk(const std::vector<common::acl::tree_chunk_8bit_t>& from_chunks,
	                              const unsigned int from_chunk_id,
	                              const unsigned int extended_chunk_id)
	{
		const auto& from_chunk = from_chunks[from_chunk_id];
		auto& extended_chunk = pointer->extended_chunks[extended_chunk_id];

		for (uint32_t i = 0;
		     i < (1u << 8);
		     i++)
		{
			const auto& from_chunk_value = from_chunk.values[i];
			auto& extended_chunk_value = extended_chunk.values[i];

			if (from_chunk_value.is_chunk_id())
			{
				YANET_LOG_ERROR("is_chunk_id\n");
				return eResult::invalidArguments;
			}
			else
			{
				extended_chunk_value.id = from_chunk_value.get_group_id();
			}
		}

		return eResult::success;
	}

protected:
	std::string name;
	dataplane::memory_manager* memory_manager;
	tSocketId socket_id;

	uint32_t extended_chunks_size;
	unsigned int extended_chunks_count;
	std::vector<unsigned int> remap_chunks;

public:
	object_type* pointer;
};

template<typename key_t,
         uint32_t chunk_size,
         unsigned int valid_bit_offset = 0,
         hash_function_t<key_t> calculate_hash = calculate_hash_crc<key_t>>
class updater_hashtable_mod_id32
{
public:
	using object_type = hashtable_mod_id32_dynamic<key_t, chunk_size, valid_bit_offset, calculate_hash>;

	updater_hashtable_mod_id32(const char* name,
	                           dataplane::memory_manager* memory_manager,
	                           const tSocketId socket_id) :
	        name(name),
	        memory_manager(memory_manager),
	        socket_id(socket_id),
	        pairs_size(0),
	        pointer(nullptr)
	{
	}

	eResult update(const std::vector<std::tuple<key_t, uint32_t>>& values)
	{
		pairs_size = upper_power_of_two(std::max(object_type::pairs_size_min,
		                                         (uint32_t)(4ull * values.size())));

		for (;;)
		{
			pointer = memory_manager->create<object_type>(name.data(),
			                                              socket_id,
			                                              object_type::calculate_sizeof(pairs_size),
			                                              pairs_size);
			if (pointer == nullptr)
			{
				return eResult::errorAllocatingMemory;
			}

			eResult result = pointer->fill(stats, values);
			if (result != eResult::success)
			{
				/// try again
				clear();
				pairs_size *= 2;
				continue;
			}

			break;
		}

		return eResult::success;
	}

	void clear()
	{
		if (pointer)
		{
			memory_manager->free(pointer);
			pointer = nullptr;
		}
	}

	void limits(limits::response& limits) const
	{
		limits.emplace_back(name + ".keys",
		                    socket_id,
		                    stats.pairs_count,
		                    pairs_size);
		limits.emplace_back(name + ".longest_collision",
		                    socket_id,
		                    stats.longest_chain,
		                    chunk_size);
	}

	void report(nlohmann::json& report) const
	{
		report["pointer"] = (uint64_t)pointer; ///< XXX: TO_HEX
		report["pairs_count"] = stats.pairs_count;
		report["pairs_size"] = pairs_size;
		for (unsigned int i = 0;
		     i < stats.pairs_in_chunks.size();
		     i++)
		{
			report["pairs_in_chunks"][i] = stats.pairs_in_chunks[i];
		}
		report["longest_chain"] = stats.longest_chain;
		report["insert_failed"] = stats.insert_failed;
		report["rewrites"] = stats.rewrites;
	}

protected:
	std::string name;
	dataplane::memory_manager* memory_manager;
	tSocketId socket_id;

	uint32_t pairs_size;
	typename object_type::stats_t stats;

public:
	object_type* pointer;
};

}
