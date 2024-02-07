#pragma once

#include <mutex>

#include <rte_malloc.h>

#include "common.h"
#include "hashtable.h"

namespace dataplane
{

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

template<typename key_t,
         uint32_t chunk_size,
         unsigned int valid_bit_offset = 0>
class updater_hashtable_mod_id32 : public updater_base
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
			                              (uint64_t)keys.size());
			while (keys_count < keys_size)
			{
				keys_count <<= 1;
			}
		}

		for (;;)
		{
			result = alloc(hashtable_t::calculate_sizeof(keys_count));
			if (result != eResult::success)
			{
				return result;
			}

			get_pointer()->total_mask = keys_count - 1;
			total_size = keys_count;

			result = fill(keys);
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
