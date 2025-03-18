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

	const Queue& Wait(const VkFence& fence) const;

	const Queue& Submit(const VkCommandBuffer& cmd) const;

	const Queue& Submit(const VkCommandBuffer& cmd, const VkFence& fence) const;

	const Queue& Submit(const VkCommandBuffer& cmd, const VkSemaphore& semaphore) const;

	const Queue& Submit(const VkCommandBuffer& cmd, const VkFence& fence, const VkSemaphore& semaphore) const;

	const Queue& Submit2(const VkCommandBuffer& cmd, const VkSemaphore& semaphore) const;

	const Queue& Submit2(const VkCommandBuffer& cmd, const VkFence& fence, const VkSemaphore& semaphore) const;

	void AllocateCommandBuffers(uint32_t count, VkCommandBuffer* outBuffers) const;

	void AllocateCommandBuffers2(uint32_t count, VkCommandBuffer* outBuffers) const;

	void FreeCommandBuffers(uint32_t count, VkCommandBuffer* buffers) const;

	const VkQueue& GetQueue() const { return queue; };

	const uint32_t& GetFamilyIndex() const { return family; };

private:
	uint32_t family = 0;
	VkQueue queue = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkCommandPool pool = VK_NULL_HANDLE;
	const VkDevice device = VK_NULL_HANDLE;
};