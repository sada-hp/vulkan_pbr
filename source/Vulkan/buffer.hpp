#pragma once
#include <array>
#include <vector>
#include <glfw/glfw3.h>
#include "Vulkan/scope.hpp"
#include <vma/vk_mem_alloc.h>

struct Buffer
{
public:
	Buffer(const RenderScope& Scope, const VkBufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo);

	Buffer(const Buffer& other) = delete;

	void operator=(const Buffer& other) = delete;

	Buffer(Buffer&& other) noexcept
		: Scope(other.Scope), buffer(std::move(other.buffer)), memory(std::move(other.memory)), allocInfo(std::move(allocInfo)), descriptorInfo(std::move(other.descriptorInfo))
	{
		other.buffer = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}

	void operator=(Buffer&& other) noexcept {
		Scope = other.Scope;
		other.buffer = std::move(other.buffer);
		memory = std::move(other.memory);
		allocInfo = std::move(other.allocInfo);
		descriptorInfo = std::move(other.descriptorInfo);

		other.buffer = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}

	~Buffer();

	const VkBuffer& GetBuffer() const { return buffer; };

	const VkDescriptorBufferInfo& GetDescriptor() const { return descriptorInfo; };

	Buffer& Map();

	Buffer& UnMap();

	Buffer& Update(void* data, size_t data_size = VK_WHOLE_SIZE, size_t offset = 0);

	Buffer& Update(VkCommandBuffer cmd, void* data, size_t data_size = VK_WHOLE_SIZE, size_t offset = 0);

	uint32_t GetSize() { return allocInfo.size; };

public:
	void* mappedMemory = VK_NULL_HANDLE;

private:
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation memory = VK_NULL_HANDLE;
	VmaAllocationInfo allocInfo = {};
	VkDescriptorBufferInfo descriptorInfo = {};
	VkBool32 gpuOnly = VK_FALSE;

	const RenderScope* Scope = VK_NULL_HANDLE;
};