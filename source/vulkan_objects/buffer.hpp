#pragma once
#include <glfw/glfw3.h>
#include <vma/vk_mem_alloc.h>
#include <vector>
#include <array>
#include "scope.hpp"

struct Buffer
{
	Buffer(const RenderScope& Scope, const VkBufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo);
	Buffer(const Buffer& other) = delete;
	void operator=(const Buffer& other) = delete;
	Buffer(Buffer&& other) noexcept
		: Scope(other.Scope), buffer(other.buffer), memory(other.memory), allocInfo(std::move(allocInfo)), descriptorInfo(std::move(other.descriptorInfo))
	{
		other.buffer = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}
	void operator=(Buffer&& other) = delete;
	~Buffer();

	const VkBuffer& GetBuffer() const { return buffer; };

	const VkDescriptorBufferInfo& GetDescriptor() const { return descriptorInfo; };

	Buffer& Map();

	Buffer& UnMap();

	Buffer& Update(void* data, size_t data_size = VK_WHOLE_SIZE);

	uint32_t GetSize() { return allocInfo.size; };

private:
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation memory = VK_NULL_HANDLE;
	VmaAllocationInfo allocInfo = {};
	VkDescriptorBufferInfo descriptorInfo = {};
	void* _mapped = VK_NULL_HANDLE;

	const RenderScope& Scope;
};