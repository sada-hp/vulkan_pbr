#include "pch.hpp"
#include "buffer.hpp"

Buffer::Buffer(const RenderScope& InScope, const VkBufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo)
	: Scope(InScope)
{
	VkBool32 res = vmaCreateBuffer(Scope.GetAllocator(), &createInfo, &allocCreateInfo, &buffer, &memory, &allocInfo) == VK_SUCCESS;
	descriptorInfo.buffer = buffer;
	descriptorInfo.offset = 0;
	descriptorInfo.range = createInfo.size;

	if ((allocCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0) {
		_mapped = allocInfo.pMappedData;
	}

	assert(res);
}

Buffer::~Buffer()
{
	if (_mapped && !allocInfo.pMappedData)
		vmaUnmapMemory(Scope.GetAllocator(), memory);
	if (buffer != VK_NULL_HANDLE)
		vmaDestroyBuffer(Scope.GetAllocator(), buffer, memory);

	allocInfo = {};
	descriptorInfo = {};

	buffer = VK_NULL_HANDLE;
	memory = VK_NULL_HANDLE;
	_mapped = VK_NULL_HANDLE;
}

Buffer& Buffer::Map()
{
	if (!_mapped)
		vmaMapMemory(Scope.GetAllocator(), memory, &_mapped);

	return *this;
}

Buffer& Buffer::UnMap()
{
	if (_mapped)
		vmaUnmapMemory(Scope.GetAllocator(), memory);

	_mapped = VK_NULL_HANDLE;
	return *this;
}

Buffer& Buffer::Update(void* data, size_t data_size)
{
	if (data_size == VK_WHOLE_SIZE)
		data_size = allocInfo.size;

	memcpy(_mapped, data, data_size);
	vmaFlushAllocation(Scope.GetAllocator(), memory, 0, data_size);

	return *this;
}