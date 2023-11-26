#include "pch.hpp"
#include "image.hpp"

Image::Image(const RenderScope& InScope)
	: Scope(&InScope)
{
}

Image::~Image()
{
	if (sampler != VK_NULL_HANDLE)
		vkDestroySampler(Scope->GetDevice(), sampler, VK_NULL_HANDLE);
	if (view != VK_NULL_HANDLE)
		vkDestroyImageView(Scope->GetDevice(), view, VK_NULL_HANDLE);
	if (image != VK_NULL_HANDLE)
		vmaDestroyImage(Scope->GetAllocator(), image, memory);

	image = VK_NULL_HANDLE;
	view = VK_NULL_HANDLE;
	sampler = VK_NULL_HANDLE;
	memory = VK_NULL_HANDLE;
}

Image& Image::CreateImage(const VkImageCreateInfo& imgInfo, const VmaAllocationCreateInfo& allocCreateInfo)
{
	if (image != VK_NULL_HANDLE)
		vmaDestroyImage(Scope->GetAllocator(), image, memory);

	VkBool32 res = vmaCreateImage(Scope->GetAllocator(), &imgInfo, &allocCreateInfo, &image, &memory, &allocInfo) == VK_SUCCESS;
	descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	subRange = {};
	imageSize = imgInfo.extent;

	assert(res);
	return *this;
}

Image& Image::CreateImageView(const VkImageViewCreateInfo& viewInfo)
{
	if (view != VK_NULL_HANDLE)
		vkDestroyImageView(Scope->GetDevice(), view, VK_NULL_HANDLE);

	VkImageViewCreateInfo Info = viewInfo;
	Info.image = image;
	VkBool32 res = vkCreateImageView(Scope->GetDevice(), &Info, VK_NULL_HANDLE, &view) == VK_SUCCESS;
	descriptorInfo.imageView = view;
	subRange = viewInfo.subresourceRange;

	assert(res);
	return *this;
}

Image& Image::CreateSampler(const VkSamplerCreateInfo& samplerInfo)
{
	if (sampler != VK_NULL_HANDLE)
		vkDestroySampler(Scope->GetDevice(), sampler, VK_NULL_HANDLE);

	VkBool32 res = vkCreateSampler(Scope->GetDevice(), &samplerInfo, VK_NULL_HANDLE, &sampler) == VK_SUCCESS;
	descriptorInfo.sampler = sampler;

	assert(res);
	return *this;
}