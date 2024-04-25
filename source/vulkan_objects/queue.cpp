#include "queue.hpp"
#include "vulkan_api.hpp"

Queue::Queue(const VkDevice& inDevice, uint32_t inFamily)
	: device(inDevice), family(inFamily)
{
	vkGetDeviceQueue(device, family, 0, &queue);

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	//fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	vkCreateFence(device, &fenceInfo, VK_NULL_HANDLE, &fence);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = family;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	vkCreateCommandPool(device, &poolInfo, VK_NULL_HANDLE, &pool);
}

Queue::~Queue()
{
	vkDestroyFence(device, fence, VK_NULL_HANDLE);
	vkDestroyCommandPool(device, pool, VK_NULL_HANDLE);
}

const Queue& Queue::Wait() const
{
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
	return *this;
}

const Queue& Queue::Submit(const VkCommandBuffer& cmd) const
{
	vkResetFences(device, 1, &fence);
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	vkQueueSubmit(queue, 1, &submitInfo, fence);
	return *this;
}

void Queue::AllocateCommandBuffers(uint32_t count, VkCommandBuffer* outBuffers) const
{
	::AllocateCommandBuffers(device, pool, count, outBuffers);
}

void Queue::FreeCommandBuffers(uint32_t count, VkCommandBuffer* buffers) const
{
	vkFreeCommandBuffers(device, pool, count, buffers);
}