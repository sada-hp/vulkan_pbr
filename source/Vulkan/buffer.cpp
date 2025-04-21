#include "pch.hpp"
#include "buffer.hpp"

Buffer::Buffer(const RenderScope& InScope, const VkBufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo)
	: Scope(&InScope)
{
	gpuOnly = allocCreateInfo.usage == VMA_MEMORY_USAGE_GPU_ONLY;

	if (gpuOnly)
		const_cast<VkBufferCreateInfo*>(&createInfo)->usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

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

Buffer& Buffer::Update(void* data, size_t data_size, size_t offset)
{
	if (data_size == VK_WHOLE_SIZE)
		data_size = allocInfo.size;

	if (gpuOnly)
	{
		VmaAllocationCreateInfo bufallocCreateInfo{};
		bufallocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		bufallocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

		VkBufferCreateInfo bufInfo{};
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufInfo.size = data_size;
		bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		std::unique_ptr<Buffer> stageBuffer = std::make_unique<Buffer>(*Scope, bufInfo, bufallocCreateInfo);
		memcpy(stageBuffer->mappedMemory, data, data_size);

		VkCommandBuffer cmd;
		Scope->GetQueue(VK_QUEUE_TRANSFER_BIT)
			.AllocateCommandBuffers(1, &cmd);

		::BeginOneTimeSubmitCmd(cmd);

		VkBufferCopy region{};
		region.size = bufInfo.size;
		region.dstOffset = offset;
		region.srcOffset = 0;
		vkCmdCopyBuffer(cmd, stageBuffer->GetBuffer(), buffer, 1, &region);

		::EndCommandBuffer(cmd);
		Scope->GetQueue(VK_QUEUE_TRANSFER_BIT)
			.Submit(cmd)
			.Wait()
			.FreeCommandBuffers(1, &cmd);
	}
	else
	{
		if (!mappedMemory)
		{
			Map();
			memcpy(((char*)mappedMemory) + offset, data, data_size);
			UnMap();
		}
		else
		{
			memcpy(((char*)mappedMemory + offset), data, data_size);
		}
	}
	vmaFlushAllocation(Scope->GetAllocator(), memory, offset, data_size);

	return *this;
}

Buffer& Buffer::Update(VkCommandBuffer cmd, void* data, size_t data_size, size_t offset)
{
	if (data_size == VK_WHOLE_SIZE)
		data_size = allocInfo.size;

	vkCmdUpdateBuffer(cmd, buffer, offset, data_size, data);

	return *this;
}
