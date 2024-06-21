#include "pch.hpp"
#include "buffer.hpp"

Buffer::Buffer(const RenderScope& InScope, const VkBufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo)
	: Scope(&InScope)
{
	VkBool32 res = vmaCreateBuffer(Scope->GetAllocator(), &createInfo, &allocCreateInfo, &buffer, &memory, &allocInfo) == VK_SUCCESS;
	descriptorInfo.buffer = buffer;
	descriptorInfo.offset = 0;
	descriptorInfo.range = createInfo.size;

	if ((allocCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0) {
		mappedMemory = allocInfo.pMappedData;
	}

	assert(res);
}

Buffer::~Buffer()
{
	if (mappedMemory && !allocInfo.pMappedData)
		vmaUnmapMemory(Scope->GetAllocator(), memory);
	if (buffer != VK_NULL_HANDLE)
		vmaDestroyBuffer(Scope->GetAllocator(), buffer, memory);

	allocInfo = {};
	descriptorInfo = {};

	buffer = VK_NULL_HANDLE;
	memory = VK_NULL_HANDLE;
	mappedMemory = VK_NULL_HANDLE;
}

Buffer& Buffer::Map()
{
	if (!mappedMemory)
		vmaMapMemory(Scope->GetAllocator(), memory, &mappedMemory);

	return *this;
}

Buffer& Buffer::UnMap()
{
	if (mappedMemory)
		vmaUnmapMemory(Scope->GetAllocator(), memory);

	mappedMemory = VK_NULL_HANDLE;
	return *this;
}

Buffer& Buffer::Update(void* data, size_t data_size)
{
	if (data_size == VK_WHOLE_SIZE)
		data_size = allocInfo.size;

	if (!mappedMemory)
	{
		Map();
		memcpy(mappedMemory, data, data_size);
		UnMap();
	}
	else
	{
		memcpy(mappedMemory, data, data_size);
	}

	vmaFlushAllocation(Scope->GetAllocator(), memory, 0, data_size);

	return *this;
}