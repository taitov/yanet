#include "updater.h"
#include "memory_manager.h"

using namespace dataplane;

updater_base::updater_base() :
        memory_manager(nullptr)
{
}

updater_base::~updater_base()
{
	clear();
}

eResult updater_base::init(const char* name,
                           dataplane::memory_manager* memory_manager)
{
	this->name = name;
	this->memory_manager = memory_manager;
	return eResult::success;
}

eResult updater_base::alloc(const tSocketId socket_id,
                            const uint64_t memory_size)
{
	/// free memory on socket_id if exist
	free(socket_id);

	auto* pointer = memory_manager->alloc(name.data(), socket_id, memory_size);
	if (pointer == nullptr)
	{
		return eResult::errorAllocatingMemory;
	}

	pointers[socket_id] = pointer;

	return eResult::success;
}

void* updater_base::get_pointer(const tSocketId socket_id)
{
	auto it = pointers.find(socket_id);
	if (it != pointers.end())
	{
		return it->second;
	}

	return nullptr;
}

const void* updater_base::get_pointer(const tSocketId socket_id) const
{
	auto it = pointers.find(socket_id);
	if (it != pointers.end())
	{
		return it->second;
	}

	return nullptr;
}

void updater_base::free(const tSocketId socket_id)
{
	auto it = pointers.find(socket_id);
	if (it != pointers.end())
	{
		memory_manager->free(it->second);
		pointers.erase(it);
	}
}

void updater_base::clear()
{
	for (auto& [socket_id, pointer] : pointers)
	{
		(void)socket_id;
		memory_manager->free(pointer);
	}
	pointers.clear();
}
