#include "updater.h"
#include "memory_manager.h"

using namespace dataplane;

updater_base::updater_base() :
        memory_manager(nullptr),
        pointer(nullptr)
{
}

updater_base::~updater_base()
{
	free();
}

eResult updater_base::init(const char* name,
                           dataplane::memory_manager* memory_manager,
                           const tSocketId socket_id)
{
	this->name = name;
	this->memory_manager = memory_manager;
	this->socket_id = socket_id;
	return eResult::success;
}

eResult updater_base::alloc(const uint64_t memory_size)
{
	/// free memory on socket_id if exist
	free();

	pointer = memory_manager->alloc(name.data(), socket_id, memory_size);
	if (pointer == nullptr)
	{
		return eResult::errorAllocatingMemory;
	}

	return eResult::success;
}

void* updater_base::get_pointer()
{
	return pointer;
}

const void* updater_base::get_pointer() const
{
	return pointer;
}

void updater_base::free()
{
	if (pointer)
	{
		memory_manager->free(pointer);
		pointer = nullptr;
	}
}
