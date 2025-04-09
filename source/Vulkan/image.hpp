#pragma once
#include <glfw/glfw3.h>
#include <vma/vk_mem_alloc.h>
#include <vector>
#include <array>
#include "Vulkan/scope.hpp"
#include "Engine/structs.hpp"

struct VulkanImage
{
	VulkanImage(const RenderScope& Scope);

	VulkanImage(const RenderScope& Scope, VkImageCreateInfo imgInfo, VmaAllocationCreateInfo allocCreateInfo);

	VulkanImage(const VulkanImage& other) = delete;

	void operator=(const VulkanImage& other) = delete;

	VulkanImage(VulkanImage&& other) noexcept
		: Scope(other.Scope), image(std::move(other.image)), memory(std::move(other.memory)),
		allocInfo(std::move(other.allocInfo)), descriptorInfo(std::move(other.descriptorInfo))
	{
		other.image = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}

	void operator=(VulkanImage&& other) noexcept {
		Scope = other.Scope;
		image = std::move(other.image);
		memory = std::move(other.memory);
		allocInfo = std::move(other.allocInfo);
		descriptorInfo = std::move(other.descriptorInfo);

		other.image = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}

	~VulkanImage();

	VulkanImage& CreateImage(VkImageCreateInfo imgInfo, VmaAllocationCreateInfo allocCreateInfo);

	VulkanImage& TransitionLayout(VkImageLayout newLayout, VkQueueFlagBits Queue = VK_QUEUE_GRAPHICS_BIT);

	VulkanImage& TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout, VkQueueFlagBits Queue);

	VulkanImage& TransitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout, VkQueueFlagBits Queue);

	VulkanImage& TransitionLayout(VkCommandBuffer cmd, VkImageSubresourceRange subResource, VkImageLayout oldLayout, VkImageLayout newLayout, VkQueueFlagBits Queue);

	VulkanImage& TransferOwnership(VkCommandBuffer cmd1, VkCommandBuffer cmd2, uint32_t queue1, uint32_t queue2);

	VulkanImage& GenerateMipMaps();

	VulkanImage& GenerateMipMaps(VkCommandBuffer cmd);

	const VkImage& GetImage() const { return image; };

	const VkImageSubresourceRange& GetSubResourceRange() const { return subRange; };

	const VkFormat& GetFormat() const { return imageFormat; };

	const VkImageType GetImageType() const { return imageType; };

	const VkExtent3D& GetExtent() const { return imageSize; };

	const uint32_t GetMipLevelsCount() const { return subRange.levelCount; };

	const uint32_t GetArrayLayers() const { return subRange.layerCount; };

	const VkImageLayout GetImageLayout() const { return descriptorInfo.imageLayout; };

	const VkImageCreateFlags GetImageFlags() const { return flags; };

private:
	VkImage image = VK_NULL_HANDLE;
	VmaAllocation memory = VK_NULL_HANDLE;

	VmaAllocationInfo allocInfo = {};
	VkDescriptorImageInfo descriptorInfo = {};
	VkImageSubresourceRange subRange = {};
	VkExtent3D imageSize = {};
	VkFormat imageFormat = {};
	VkImageType imageType = {};
	VkImageCreateFlags flags;

	const RenderScope* Scope = VK_NULL_HANDLE;
};

struct VulkanImageView
{
public:
	VulkanImageView(const RenderScope& Scope);

	VulkanImageView(const RenderScope& Scope, const VulkanImage& Image);

	VulkanImageView(const RenderScope& Scope, const VulkanImage& Image, const VkImageSubresourceRange& SubResource);

	VulkanImageView(const VulkanImageView& other) = delete;

	void operator=(const VulkanImageView& other) = delete;

	VulkanImageView(VulkanImageView&& other) noexcept
		: Scope(other.Scope), m_View(std::move(other.m_View)), subRange(std::move(other.subRange))
	{
		other.m_View = VK_NULL_HANDLE;
		other.subRange = {};
	}

	void operator=(VulkanImageView&& other) noexcept {
		Scope = other.Scope;
		m_View = std::move(other.m_View);
		subRange = std::move(other.subRange);

		other.m_View = VK_NULL_HANDLE;
		other.subRange = {};
	}

	VulkanImageView& CreateImageView(const VulkanImage& Image, const VkImageSubresourceRange& SubResource);

	VkImageView GetImageView() const { return m_View; };

	const VkImageSubresourceRange& GetSubresourceRange() const { return subRange; };

	~VulkanImageView();

private:
	VkImageView m_View = VK_NULL_HANDLE;
	const RenderScope* Scope = VK_NULL_HANDLE;
	VkImageSubresourceRange subRange = {};
};