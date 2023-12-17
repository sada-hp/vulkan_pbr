#include "pch.hpp"
#include "image.hpp"

Image::Image(const RenderScope& InScope)
	: Scope(&InScope)
{
}

Image::Image(const RenderScope& InScope, const VkImageCreateInfo& imgInfo, const VmaAllocationCreateInfo& allocCreateInfo)
	: Scope(&InScope)
{
	CreateImage(imgInfo, allocCreateInfo);
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
	descriptorInfo.imageLayout = imgInfo.initialLayout;
	subRange.levelCount = imgInfo.arrayLayers;
	subRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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

Image& Image::TransitionLayout(VkCommandBuffer& cmd, VkImageLayout newLayout)
{
	const std::unordered_map<VkImageLayout, VkPipelineStageFlags> stageTable = {
		{VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT},
		{VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT},
		{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
		{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
		{VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT},
		{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT},
		{VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
		{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT}
	};

	const std::unordered_map<VkImageLayout, VkAccessFlags> accessTable = {
		{VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_NONE_KHR},
		{VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE_KHR},
		{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
		{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
		{VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
		{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
		{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_MEMORY_READ_BIT},
		{VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT}
	};

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = descriptorInfo.imageLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = subRange;
	barrier.srcAccessMask = accessTable.at(barrier.oldLayout);
	barrier.dstAccessMask = accessTable.at(barrier.newLayout);
	vkCmdPipelineBarrier(cmd, stageTable.at(barrier.oldLayout), stageTable.at(barrier.newLayout), 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

	descriptorInfo.imageLayout = newLayout;

	return *this;
}

Image& Image::GenerateMipMaps(VkCommandBuffer& cmd)
{
	if (subRange.levelCount == 1) {
		TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		return *this;
	}

	TransitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	for (int k = 0; k < subRange.layerCount; k++) {
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = subRange.aspectMask;
		barrier.subresourceRange.baseArrayLayer = k;
		barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;

		int32_t mipWidth = imageSize.width;
		int32_t mipHeight = imageSize.height;

		for (uint32_t i = 1; i < subRange.levelCount; i++) {
			barrier.subresourceRange.baseMipLevel = i;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = subRange.aspectMask;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = k;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = subRange.aspectMask;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = k;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(cmd,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = subRange.levelCount;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	}

	descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	return *this;;
}