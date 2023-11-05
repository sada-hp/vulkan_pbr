#pragma once
#include <glfw/glfw3.h>
#include <vma/vk_mem_alloc.h>

class Queue
{
public:
	Queue() = delete;
	Queue(const VkDevice& device, uint32_t family);
	~Queue();

	const Queue& Wait() const;

	const Queue& Submit(const VkCommandBuffer& cmd) const;

	const Queue& AllocateCommandBuffers(uint32_t count, VkCommandBuffer* outBuffers) const;

	const Queue& FreeCommandBuffers(uint32_t count, VkCommandBuffer* buffers) const;

	const VkQueue& GetQueue() const { return queue; };

	const uint32_t& GetFamilyIndex() const { return family; };

private:
	uint32_t family = 0;
	VkQueue queue = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkCommandPool pool = VK_NULL_HANDLE;
	const VkDevice device = VK_NULL_HANDLE;

	mutable int allocated_buffers = 0;
};