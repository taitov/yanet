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

class updater_lpm4_24bit_8bit_id32 : public updater<lpm4_24bit_8bit_id32_dynamic>
{
public:
	updater_lpm4_24bit_8bit_id32(const char* name,
	                             dataplane::memory_manager* memory_manager,
	                             const tSocketId socket_id) :
	        updater<lpm4_24bit_8bit_id32_dynamic>::updater(name, memory_manager, socket_id)
	{
	}

	eResult update(const std::vector<common::acl::tree_chunk_8bit_t>& values)
	{
		(void)values;
		return eResult::dataplaneIsBroken;
	}
};

template<typename key_t,
         uint32_t chunk_size,
         unsigned int valid_bit_offset = 0>
class updater_hashtable_mod_id32
{
public:
	using object_type = hashtable_mod_id32_dynamic<key_t, chunk_size, valid_bit_offset>;

	updater_hashtable_mod_id32(const char* name,
	                           dataplane::memory_manager* memory_manager,
	                           const tSocketId socket_id) :
	        name(name),
	        memory_manager(memory_manager),
	        socket_id(socket_id),
	        keys_count(0),
	        keys_size(0),
	        pointer(nullptr)
	{
	}

	static uint64_t calculate_sizeof(const uint64_t keys_size)
	{
		if (!keys_size)
		{
			YANET_LOG_ERROR("wrong keys_size: %lu\n", keys_size);
			return 0;
		}
		else if (keys_size > 0xFFFFFFFFull)
		{
			YANET_LOG_ERROR("wrong keys_size: %lu\n", keys_size);
			return 0;
		}

		if (__builtin_popcount(keys_size) != 1)
		{
			YANET_LOG_ERROR("wrong keys_size: %lu is non power of 2\n", keys_size);
			return 0;
		}

		return sizeof(object_type) + keys_size * sizeof(typename object_type::pair);
	}

	eResult update(const std::vector<std::tuple<key_t, uint32_t>>& values)
	{
		constexpr uint64_t keys_size_min = 128;

		keys_size = 1;
		{
			uint64_t keys_size_need = std::max(keys_size_min,
			                                   (uint64_t)(4ull * values.size()));
			while (keys_size < keys_size_need)
			{
				keys_size <<= 1;
			}
		}

		for (;;)
		{
			pointer = memory_manager->create<object_type>(name.data(),
			                                              socket_id,
			                                              calculate_sizeof(keys_size),
			                                              keys_size);
			if (pointer == nullptr)
			{
				return eResult::errorAllocatingMemory;
			}

			eResult result = fill(values);
			if (result != eResult::success)
			{
				/// try again

				keys_size <<= 1;
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
		                    keys_count,
		                    keys_size);
		limits.emplace_back(name + ".longest_collision",
		                    socket_id,
		                    longest_chain,
		                    chunk_size);
	}

	void report(nlohmann::json& report) const
	{
		report["pointer"] = (uint64_t)pointer; ///< XXX: TO_HEX
		report["keys_count"] = keys_count;
		report["keys_size"] = keys_size;
		for (unsigned int i = 0;
		     i < keys_in_chunks.size();
		     i++)
		{
			report["keys_in_chunks"][i] = keys_in_chunks[i];
		}
		report["longest_chain"] = longest_chain;
		report["insert_failed"] = insert_failed;
		report["rewrites"] = rewrites;
	}

protected:
	eResult fill(const std::vector<std::tuple<key_t, uint32_t>>& values)
	{
		eResult result = eResult::success;

		keys_count = 0;
		keys_in_chunks.fill(0);
		longest_chain = 0;
		insert_failed = 0;
		rewrites = 0;

		for (const auto& [key, value] : values)
		{
			eResult insert_result = insert(key, value);
			if (insert_result != eResult::success)
			{
				result = insert_result;
			}
		}

		for (uint32_t chunk_i = 0;
		     chunk_i < keys_size / chunk_size;
		     chunk_i++)
		{
			unsigned int count = 0;

			for (uint32_t pair_i = 0;
			     pair_i < chunk_size;
			     pair_i++)
			{
				if (pointer->is_valid(chunk_i * chunk_size + pair_i))
				{
					count++;
				}
			}

			keys_in_chunks[count]++;
		}

		return result;
	}

	eResult insert(const key_t& key,
	               const uint32_t value)
	{
		const uint32_t hash = object_type::calculate_hash(key) & pointer->total_mask;

		for (unsigned int try_i = 0;
		     try_i < chunk_size;
		     try_i++)
		{
			const uint32_t index = (hash + try_i) & pointer->total_mask;

			if (!pointer->is_valid(index))
			{
				memcpy(&pointer->pairs[index].key, &key, sizeof(key_t));
				pointer->pairs[index].value = value;
				pointer->pairs[index].value |= 1u << object_type::shift_valid;

				keys_count++;

				uint64_t longest_chain = try_i + 1;
				if (this->longest_chain < longest_chain)
				{
					this->longest_chain = longest_chain;
				}

				return eResult::success;
			}
			else if (pointer->is_valid_and_equal(index, key))
			{
				pointer->pairs[index].value = value;
				pointer->pairs[index].value |= 1u << object_type::shift_valid;

				rewrites++;

				return eResult::success;
			}
		}

		insert_failed++;

		return eResult::isFull;
	}

protected:
	std::string name;
	dataplane::memory_manager* memory_manager;
	tSocketId socket_id;

	uint32_t keys_count;
	uint32_t keys_size;
	std::array<uint32_t, chunk_size + 1> keys_in_chunks;
	uint32_t longest_chain;
	uint64_t insert_failed;
	uint64_t rewrites;

public:
	object_type* pointer;
};

template<typename key_t,
         uint32_t chunk_size,
         unsigned int valid_bit_offset = 0>
class updater123321123_hashtable_mod_id32 : public updater_base
{
public:
	using hashtable_t = hashtable_mod_id32_dynamic<key_t, chunk_size, valid_bit_offset>;

	hashtable_t* get_pointer()
	{
		return reinterpret_cast<hashtable_t*>(updater_base::get_pointer());
	}

	const hashtable_t* get_pointer() const
	{
		return reinterpret_cast<const hashtable_t*>(updater_base::get_pointer());
	}

	template<typename update_key_t>
	eResult update(const std::vector<std::tuple<update_key_t, uint32_t>>& keys)
	{
		eResult result = eResult::success;

		uint64_t keys_count = 1;
		{
			constexpr uint64_t keys_size_min = 128;

			uint64_t keys_size = std::max(keys_size_min,
			                              (uint64_t)(4ull * keys.size()));
			while (keys_count < keys_size)
			{
				keys_count <<= 1;
			}
		}

		for (;;)
		{
			YANET_LOG_INFO("XXX:UPDATE:1\n");
			result = alloc(hashtable_t::calculate_sizeof(keys_count));
			YANET_LOG_INFO("XXX:UPDATE:2\n");
			if (result != eResult::success)
			{
				return result;
			}

			get_pointer()->total_mask = keys_count - 1;
			total_size = keys_count;

			YANET_LOG_INFO("XXX:UPDATE:3\n");
			for (uint32_t i = 0;
			     i < total_size;
			     i++)
			{
				get_pointer()->pairs[i].value = 0;
			}
			YANET_LOG_INFO("XXX:UPDATE:4\n");

			result = fill(keys);
			YANET_LOG_INFO("XXX:UPDATE:5\n");
			if (result != eResult::success)
			{
				/// try again

				keys_count <<= 1;
				continue;
			}

			break;
		}

		return eResult::success;
	}

	template<typename list_T> ///< @todo: common::idp::limits::response
	void limits(list_T& list) const
	{
		list.emplace_back(name + ".keys",
		                  socket_id,
		                  keys_count,
		                  total_size);
		list.emplace_back(name + ".longest_collision",
		                  socket_id,
		                  longest_chain,
		                  chunk_size);
	}

protected:
	template<typename update_key_t>
	eResult fill(const std::vector<std::tuple<update_key_t, uint32_t>>& keys)
	{
		auto* hashtable = get_pointer();

		eResult result = eResult::success;

		keys_count = 0;
		keys_in_chunks.fill(0);
		longest_chain = 0;
		insert_failed = 0;
		rewrites = 0;

		for (const auto& [key, value] : keys)
		{
			eResult insert_result;
			if constexpr (std::is_same_v<update_key_t, key_t>)
			{
				insert_result = insert(key, value);
			}
			else
			{
				insert_result = insert(key_t::convert(key), value);
			}

			if (insert_result != eResult::success)
			{
				result = insert_result;
			}
		}

		for (uint32_t chunk_i = 0;
		     chunk_i < total_size / chunk_size;
		     chunk_i++)
		{
			unsigned int count = 0;

			for (uint32_t pair_i = 0;
			     pair_i < chunk_size;
			     pair_i++)
			{
				if (hashtable->is_valid(chunk_i * chunk_size + pair_i))
				{
					count++;
				}
			}

			keys_in_chunks[count]++;
		}

		return result;
	}

	eResult insert(const key_t& key,
	               const uint32_t value)
	{
		auto* hashtable = get_pointer();

		const uint32_t hash = hashtable_t::calculate_hash(key) & hashtable->total_mask;

		for (unsigned int try_i = 0;
		     try_i < chunk_size;
		     try_i++)
		{
			const uint32_t index = (hash + try_i) % total_size;

			if (!hashtable->is_valid(index))
			{
				memcpy(&hashtable->pairs[index].key, &key, sizeof(key_t));
				hashtable->pairs[index].value = value;
				hashtable->pairs[index].value |= 1u << hashtable_t::shift_valid;

				keys_count++;

				uint64_t longest_chain = try_i + 1;
				if (this->longest_chain < longest_chain)
				{
					this->longest_chain = longest_chain;
				}

				return eResult::success;
			}
			else if (hashtable->is_valid_and_equal(index, key))
			{
				hashtable->pairs[index].value = value;
				hashtable->pairs[index].value |= 1u << hashtable_t::shift_valid;

				rewrites++;

				return eResult::success;
			}
		}

		insert_failed++;

		return eResult::isFull;
	}

public:
	uint32_t total_size;
	uint32_t keys_count;
	std::array<uint32_t, chunk_size + 1> keys_in_chunks;
	uint32_t longest_chain;
	uint64_t insert_failed;
	uint64_t rewrites;
};

}
