#pragma once
#include <glfw/glfw3.h>
#include <vma/vk_mem_alloc.h>
#include <vector>
#include <array>
#include "scope.hpp"
#include "structs.hpp"

struct VulkanImage : Image
{
	VulkanImage(const RenderScope& Scope);

	VulkanImage(const RenderScope& Scope, const VkImageCreateInfo& imgInfo, const VmaAllocationCreateInfo& allocCreateInfo);

	VulkanImage(const VulkanImage& other) = delete;

	void operator=(const VulkanImage& other) = delete;

	VulkanImage(VulkanImage&& other) noexcept
		: Scope(other.Scope), image(std::move(other.image)), view(std::move(other.view)), sampler(std::move(other.sampler)), memory(std::move(other.memory)),
		allocInfo(std::move(other.allocInfo)), descriptorInfo(std::move(other.descriptorInfo))
	{
		other.image = VK_NULL_HANDLE;
		other.view = VK_NULL_HANDLE;
		other.sampler = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}

	void operator=(VulkanImage&& other) noexcept {
		Scope = other.Scope;
		image = std::move(other.image);
		view = std::move(other.view);
		sampler = std::move(other.sampler);
		memory = std::move(other.memory);
		allocInfo = std::move(other.allocInfo);
		descriptorInfo = std::move(other.descriptorInfo);

		other.image = VK_NULL_HANDLE;
		other.view = VK_NULL_HANDLE;
		other.sampler = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}

	~VulkanImage();

	VulkanImage& CreateImage(const VkImageCreateInfo& imgInfo, const VmaAllocationCreateInfo& allocCreateInfo);

	VulkanImage& CreateImageView(const VkImageViewCreateInfo& viewInfo);

	VulkanImage& CreateSampler(ESamplerType Type);

	VulkanImage& TransitionLayout(VkImageLayout newLayout);

	VulkanImage& TransitionLayout(VkCommandBuffer& cmd, VkImageLayout newLayout);

	VulkanImage& GenerateMipMaps();

	VulkanImage& GenerateMipMaps(VkCommandBuffer& cmd);

	const VkImage& GetImage() const { return image; };

	const VkImageView& GetImageView() const { return view; };

	const VkSampler& GetSampler() const { return sampler; };

	const VkDescriptorImageInfo& GetDescriptor() const { return descriptorInfo; };

	const VkImageSubresourceRange& GetSubResourceRange() const { return subRange; };

	const VkExtent3D& GetExtent() const { return imageSize; };

private:
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VmaAllocation memory = VK_NULL_HANDLE;
	VmaAllocationInfo allocInfo = {};
	VkDescriptorImageInfo descriptorInfo = {};
	VkImageSubresourceRange subRange = {};
	VkExtent3D imageSize = {};

	const RenderScope* Scope = VK_NULL_HANDLE;
};