#pragma once
#include <glfw/glfw3.h>
#include <vma/vk_mem_alloc.h>
#include <vector>
#include <array>
#include "scope.hpp"

struct Image
{
	Image(const RenderScope& Scope);

	Image(const RenderScope& Scope, const VkImageCreateInfo& imgInfo, const VmaAllocationCreateInfo& allocCreateInfo);

	Image(const Image& other) = delete;

	void operator=(const Image& other) = delete;

	Image(Image&& other) noexcept
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

	void operator=(Image&& other) noexcept {
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

	~Image();

	Image& CreateImage(const VkImageCreateInfo& imgInfo, const VmaAllocationCreateInfo& allocCreateInfo);

	Image& CreateImageView(const VkImageViewCreateInfo& viewInfo);

	Image& CreateSampler(const VkSamplerCreateInfo& samplerInfo);

	Image& TransitionLayout(VkCommandBuffer& cmd, VkImageLayout newLayout);

	Image& GenerateMipMaps(VkCommandBuffer& cmd);

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