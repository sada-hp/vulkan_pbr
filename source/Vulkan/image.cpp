#include "pch.hpp"
#include "image.hpp"

VulkanImage::VulkanImage(const RenderScope& InScope)
	: Scope(&InScope)
{

}

VulkanImage::VulkanImage(const RenderScope& InScope, VkImageCreateInfo imgInfo, VmaAllocationCreateInfo allocCreateInfo)
	: Scope(&InScope)
{
	CreateImage(imgInfo, allocCreateInfo);
}

VulkanImage::~VulkanImage()
{
	if (image != VK_NULL_HANDLE)
		vmaDestroyImage(Scope->GetAllocator(), image, memory);
}

VulkanImage& VulkanImage::CreateImage(VkImageCreateInfo imgInfo, VmaAllocationCreateInfo allocCreateInfo)
{
	if (image != VK_NULL_HANDLE)
		vmaDestroyImage(Scope->GetAllocator(), image, memory);

	VkImageLayout layout = imgInfo.initialLayout;
	imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkBool32 res = vmaCreateImage(Scope->GetAllocator(), &imgInfo, &allocCreateInfo, &image, &memory, &allocInfo) == VK_SUCCESS;
	subRange.baseArrayLayer = 0;
	subRange.baseMipLevel = 0;
	subRange.layerCount = imgInfo.arrayLayers;
	subRange.levelCount = imgInfo.mipLevels;
	imageSize = imgInfo.extent;
	imageFormat = imgInfo.format;
	imageType = imgInfo.imageType;
	subRange.aspectMask = (imageFormat == VK_FORMAT_D16_UNORM 
		|| imageFormat == VK_FORMAT_D32_SFLOAT
		|| imageFormat == VK_FORMAT_D24_UNORM_S8_UINT
		|| imageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT
		|| imageFormat == VK_FORMAT_D16_UNORM) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	assert(res);

	if (layout != VK_IMAGE_LAYOUT_UNDEFINED)
		TransitionLayout(layout);

	return *this;
}

VulkanImage& VulkanImage::TransitionLayout(VkImageLayout newLayout, VkQueueFlagBits Queue)
{
	VkCommandBuffer cmd;
	Scope->GetQueue(Queue)
		.AllocateCommandBuffers(1, &cmd);

	::BeginOneTimeSubmitCmd(cmd);
	TransitionLayout(cmd, newLayout, Queue);
	::EndCommandBuffer(cmd);

	Scope->GetQueue(Queue)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	return *this;
}

VulkanImage& VulkanImage::TransitionLayout(VkCommandBuffer& cmd, VkImageLayout newLayout, VkQueueFlagBits Queue)
{
	if (newLayout == descriptorInfo.imageLayout)
		return *this;

	const std::unordered_map<VkImageLayout, VkPipelineStageFlags> stageTable = {
		{VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT},
		{VK_IMAGE_LAYOUT_GENERAL, (Queue == VK_QUEUE_GRAPHICS_BIT) ? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT },
		{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
		{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, (Queue == VK_QUEUE_GRAPHICS_BIT) ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT},
		{VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT},
		{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT},
		{VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
		{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT}
	};

	const std::unordered_map<VkImageLayout, VkAccessFlags> accessTable = {
		{VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_NONE_KHR},
		{VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT},
		{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
		{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
		{VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT},
		{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT},
		{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_MEMORY_READ_BIT},
		{VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT}
	};

	assert(stageTable.contains(newLayout));

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

VulkanImage& VulkanImage::GenerateMipMaps()
{
	VkCommandBuffer cmd;
	Scope->GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers(1, &cmd);

	::BeginOneTimeSubmitCmd(cmd);
	GenerateMipMaps(cmd);
	::EndCommandBuffer(cmd);

	Scope->GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	return *this;
}

VulkanImage& VulkanImage::GenerateMipMaps(VkCommandBuffer& cmd)
{
	if (subRange.levelCount == 1) {
		TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
		return *this;
	}

	TransitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
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
		int32_t mipDepth = imageSize.depth;

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
			blit.srcOffsets[1] = { mipWidth, mipHeight, mipDepth };
			blit.srcSubresource.aspectMask = subRange.aspectMask;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = k;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, mipDepth > 1 ? mipDepth / 2 : 1 };
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
			if (mipDepth > 1) mipDepth /= 2;
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

	return *this;
}

VulkanImageView::VulkanImageView(const RenderScope& InScope)
	: Scope(&InScope)
{

}

VulkanImageView::VulkanImageView(const RenderScope& InScope, const VulkanImage& Image)
	: Scope(&InScope)
{
	CreateImageView(Image, Image.GetSubResourceRange());
}

VulkanImageView::VulkanImageView(const RenderScope& InScope, const VulkanImage& Image, const VkImageSubresourceRange& SubResource)
	: Scope(&InScope)
{
	CreateImageView(Image, SubResource);
}

VulkanImageView& VulkanImageView::CreateImageView(const VulkanImage& Image, const VkImageSubresourceRange& SubResource)
{
	if (m_View != VK_NULL_HANDLE)
		vkDestroyImageView(Scope->GetDevice(), m_View, VK_NULL_HANDLE);

	VkImageViewCreateInfo Info = {};
	Info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	Info.image = Image.GetImage();
	Info.format = Image.GetFormat();
	Info.subresourceRange = SubResource;
	Info.viewType = Image.GetImageType() == VK_IMAGE_TYPE_2D ? (Image.GetSubResourceRange().layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D) : VK_IMAGE_VIEW_TYPE_3D;

	VkBool32 res = vkCreateImageView(Scope->GetDevice(), &Info, VK_NULL_HANDLE, &m_View) == VK_SUCCESS;

	assert(res);

	return *this;
}

VulkanImageView::~VulkanImageView()
{
	vkDestroyImageView(Scope->GetDevice(), m_View, VK_NULL_HANDLE);
}