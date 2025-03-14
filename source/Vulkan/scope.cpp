#include "pch.hpp"
#include "scope.hpp"

RenderScope& RenderScope::CreatePhysicalDevice(const VkInstance& instance, const std::vector<const char*>& device_extensions)
{
	assert(m_PhysicalDevice == VK_NULL_HANDLE);

	::EnumeratePhysicalDevices(instance, device_extensions, &m_PhysicalDevice);
	return *this;
}

RenderScope& RenderScope::CreateLogicalDevice(const VkPhysicalDeviceFeatures& features, const std::vector<const char*>& device_extensions, const std::vector<VkQueueFlagBits>& queues, void* pNext)
{
	assert(m_PhysicalDevice != VK_NULL_HANDLE && m_LogicalDevice == VK_NULL_HANDLE);

	::CreateLogicalDevice(m_PhysicalDevice, features, device_extensions, FindDeviceQueues(m_PhysicalDevice, queues), &m_LogicalDevice, pNext);

	for (const auto& queue : queues) {
		uint32_t queueFamilies = FindDeviceQueues(m_PhysicalDevice, { queue })[0];
		m_Queues.emplace(std::piecewise_construct, std::forward_as_tuple(queue), std::forward_as_tuple(m_LogicalDevice, queueFamilies));
	}
	return *this;
}

RenderScope& RenderScope::CreateMemoryAllocator(const VkInstance& instance)
{
	assert(m_PhysicalDevice != VK_NULL_HANDLE && m_LogicalDevice != VK_NULL_HANDLE && m_Allocator == VK_NULL_HANDLE);

	::CreateAllocator(instance, m_PhysicalDevice, m_LogicalDevice, &m_Allocator);

	return *this;
}

RenderScope& RenderScope::CreateSwapchain(const VkSurfaceKHR& surface)
{
	assert(m_PhysicalDevice != VK_NULL_HANDLE && m_LogicalDevice != VK_NULL_HANDLE && m_Swapchain == VK_NULL_HANDLE);

	VkSurfaceCapabilitiesKHR surfaceCapabilities{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, surface, &surfaceCapabilities);

	if (::CreateSwapchain(m_LogicalDevice, m_PhysicalDevice, surface, { GetColorFormat() , VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}, surfaceCapabilities.currentExtent, &m_Swapchain)) {
		vkGetSwapchainImagesKHR(m_LogicalDevice, m_Swapchain, &m_FramesInFlight, VK_NULL_HANDLE);
	}
	m_SwapchainExtent = surfaceCapabilities.currentExtent;

	return *this;
}

RenderScope& RenderScope::CreateDefaultRenderPass()
{
	VkRenderPassCreateInfo createInfo{};
	std::array<VkAttachmentDescription, 4> attachments;

	// HDR attachment
	attachments[0].format = GetHDRFormat();
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].flags = 0;

	// Normal attachment
	attachments[1].format = VK_FORMAT_R8G8B8A8_SNORM;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].flags = 0;

	// Deferred attachment
	attachments[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].flags = 0;

	// Depth attachment
	attachments[3].format = GetDepthFormat();
	attachments[3].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[3].flags = 0;

	std::array<VkAttachmentReference, 3> hdr_ref
	{ 
		VkAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, 
		VkAttachmentReference{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, 
		VkAttachmentReference{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL} 
	};

	std::array<VkAttachmentReference, 1> depth_ref
	{ 
		VkAttachmentReference{3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL} 
	};

	std::array<VkSubpassDescription, 1> subpassDescriptions{};
	subpassDescriptions[0].colorAttachmentCount = hdr_ref.size();
	subpassDescriptions[0].pColorAttachments = hdr_ref.data();
	subpassDescriptions[0].pDepthStencilAttachment = depth_ref.data();
	subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = attachments.size();
	createInfo.pAttachments = attachments.data();
	createInfo.dependencyCount = 0;
	createInfo.pDependencies = VK_NULL_HANDLE;
	createInfo.subpassCount = subpassDescriptions.size();
	createInfo.pSubpasses = subpassDescriptions.data();

	::CreateRenderPass(m_LogicalDevice, createInfo, &m_RenderPass);

	return *this;
}

RenderScope& RenderScope::CreateCompositionRenderPass()
{
	VkRenderPassCreateInfo createInfo{};
	std::array<VkAttachmentDescription, 1> attachments;

	attachments[0].format = GetHDRFormat();
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].flags = 0;

	VkAttachmentReference color_ref{ 0, VK_IMAGE_LAYOUT_GENERAL };

	std::array<VkSubpassDescription, 2> subpassDescriptions{};
	subpassDescriptions[0].colorAttachmentCount = 1;
	subpassDescriptions[0].pColorAttachments = &color_ref;
	subpassDescriptions[0].pDepthStencilAttachment = nullptr;
	subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	subpassDescriptions[1].colorAttachmentCount = 1;
	subpassDescriptions[1].pColorAttachments = &color_ref;
	subpassDescriptions[1].pDepthStencilAttachment = nullptr;
	subpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescriptions[1].inputAttachmentCount = 1;
	subpassDescriptions[1].pInputAttachments = &color_ref;

	std::array<VkSubpassDependency, 3> dependencies;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[2].srcSubpass = 1;
	dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = attachments.size();
	createInfo.pAttachments = attachments.data();
	createInfo.dependencyCount = dependencies.size();
	createInfo.pDependencies = dependencies.data();
	createInfo.subpassCount = subpassDescriptions.size();
	createInfo.pSubpasses = subpassDescriptions.data();

	::CreateRenderPass(m_LogicalDevice, createInfo, &m_CompositionPass);

	return *this;
}

RenderScope& RenderScope::CreateLowResRenderPass()
{
	VkRenderPassCreateInfo createInfo{};
	std::array<VkAttachmentDescription, 2> attachments;

	//HDR attachment
	attachments[0].format = GetHDRFormat();
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].flags = 0;

	//Depth attachment
	attachments[1].format = GetDepthFormat();
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].flags = 0;

	VkAttachmentReference hdr_ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depth_ref{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	std::array<VkSubpassDescription, 1> subpassDescriptions{};
	subpassDescriptions[0].colorAttachmentCount = 1;
	subpassDescriptions[0].pColorAttachments = &hdr_ref;
	subpassDescriptions[0].pDepthStencilAttachment = &depth_ref;
	subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = attachments.size();
	createInfo.pAttachments = attachments.data();
	createInfo.dependencyCount = 0;
	createInfo.pDependencies = VK_NULL_HANDLE;
	createInfo.subpassCount = subpassDescriptions.size();
	createInfo.pSubpasses = subpassDescriptions.data();

	::CreateRenderPass(m_LogicalDevice, createInfo, &m_RenderPassLR);

	return *this;
}

RenderScope& RenderScope::CreatePostProcessRenderPass()
{
	VkRenderPassCreateInfo createInfo{};
	std::array<VkAttachmentDescription, 1> attachments;

	attachments[0].format = GetColorFormat();
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].flags = 0;

	VkAttachmentReference color_ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	std::array<VkSubpassDescription, 2> subpassDescriptions{};
	subpassDescriptions[0].colorAttachmentCount = 1;
	subpassDescriptions[0].pColorAttachments = &color_ref;
	subpassDescriptions[0].pDepthStencilAttachment = nullptr;
	subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	subpassDescriptions[1].colorAttachmentCount = 1;
	subpassDescriptions[1].pColorAttachments = &color_ref;
	subpassDescriptions[1].pDepthStencilAttachment = nullptr;
	subpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	std::array<VkSubpassDependency, 3> dependencies;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[2].srcSubpass = 1;
	dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = attachments.size();
	createInfo.pAttachments = attachments.data();
	createInfo.dependencyCount = dependencies.size();
	createInfo.pDependencies = dependencies.data();
	createInfo.subpassCount = subpassDescriptions.size();
	createInfo.pSubpasses = subpassDescriptions.data();

	::CreateRenderPass(m_LogicalDevice, createInfo, &m_PostProcessPass);

	return *this;
}

RenderScope& RenderScope::CreateDescriptorPool(uint32_t setsCount, const std::vector<VkDescriptorPoolSize>& poolSizes)
{
	if (m_DescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(m_LogicalDevice, m_DescriptorPool, VK_NULL_HANDLE);
	}

	::CreateDescriptorPool(m_LogicalDevice, poolSizes.data(), poolSizes.size(), setsCount, &m_DescriptorPool);

	return *this;
}

void RenderScope::RecreateSwapchain(const VkSurfaceKHR& surface)
{
	vkDestroySwapchainKHR(m_LogicalDevice, m_Swapchain, VK_NULL_HANDLE);
	m_Swapchain = VK_NULL_HANDLE;

	CreateSwapchain(surface);
}

void RenderScope::Destroy()
{
	m_Queues.clear();

	for (auto pair : m_Samplers)
		vkDestroySampler(m_LogicalDevice, pair.second, VK_NULL_HANDLE);
	m_Samplers.clear();

	if (m_DescriptorPool != VK_NULL_HANDLE)
		vkDestroyDescriptorPool(m_LogicalDevice, m_DescriptorPool, VK_NULL_HANDLE);
	if (m_RenderPass != VK_NULL_HANDLE)
		vkDestroyRenderPass(m_LogicalDevice, m_RenderPass, VK_NULL_HANDLE);
	if (m_RenderPassLR != VK_NULL_HANDLE)
		vkDestroyRenderPass(m_LogicalDevice, m_RenderPassLR, VK_NULL_HANDLE);
	if (m_CompositionPass != VK_NULL_HANDLE)
		vkDestroyRenderPass(m_LogicalDevice, m_CompositionPass, VK_NULL_HANDLE);
	if (m_PostProcessPass != VK_NULL_HANDLE)
		vkDestroyRenderPass(m_LogicalDevice, m_PostProcessPass, VK_NULL_HANDLE);
	if (m_Swapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(m_LogicalDevice, m_Swapchain, VK_NULL_HANDLE);
	if (m_Allocator != VK_NULL_HANDLE)
		vmaDestroyAllocator(m_Allocator);
	if (m_LogicalDevice != VK_NULL_HANDLE)
		vkDestroyDevice(m_LogicalDevice, VK_NULL_HANDLE);

	m_DescriptorPool = VK_NULL_HANDLE;
	m_RenderPass = VK_NULL_HANDLE;
	m_RenderPassLR = VK_NULL_HANDLE;
	m_CompositionPass = VK_NULL_HANDLE;
	m_PostProcessPass = VK_NULL_HANDLE;
	m_Swapchain = VK_NULL_HANDLE;
	m_Allocator = VK_NULL_HANDLE;
	m_LogicalDevice = VK_NULL_HANDLE;
}

VkBool32 RenderScope::IsReadyToUse() const
{
	return 
		m_LogicalDevice != VK_NULL_HANDLE
		&& m_PhysicalDevice != VK_NULL_HANDLE
		&& m_Allocator != VK_NULL_HANDLE
		&& m_Swapchain != VK_NULL_HANDLE
		&& m_RenderPass != VK_NULL_HANDLE
		&& m_RenderPassLR != VK_NULL_HANDLE
		&& m_CompositionPass != VK_NULL_HANDLE
		&& m_DescriptorPool != VK_NULL_HANDLE;
}

const VkSampler& RenderScope::GetSampler(ESamplerType Type) const
{
	if (m_Samplers.count(Type) == 0)
	{
		const bool Point = Type == ESamplerType::PointClamp || Type == ESamplerType::PointMirror || Type == ESamplerType::PointRepeat;
		const bool Repeat = Type == ESamplerType::PointRepeat || Type == ESamplerType::LinearRepeat || Type == ESamplerType::BillinearRepeat;
		const bool Mirror = Type == ESamplerType::PointMirror || Type == ESamplerType::LinearMirror || Type == ESamplerType::BillinearMirror;
		const bool Anisotropy = Type == ESamplerType::BillinearClamp || Type == ESamplerType::BillinearRepeat || Type == ESamplerType::BillinearMirror;

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = Point ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
		samplerInfo.minFilter = Point ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
		samplerInfo.addressModeU = Repeat ? (Mirror ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT : VK_SAMPLER_ADDRESS_MODE_REPEAT) : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = Repeat ? (Mirror ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT : VK_SAMPLER_ADDRESS_MODE_REPEAT) : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = Repeat ? (Mirror ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT : VK_SAMPLER_ADDRESS_MODE_REPEAT) : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		if (Anisotropy)
		{
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);

			samplerInfo.anisotropyEnable = VK_TRUE;
			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		}

		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_TRUE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = Point ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0;
		samplerInfo.minLod = 0.0;
		samplerInfo.maxLod = 1.0;
		vkCreateSampler(m_LogicalDevice, &samplerInfo, VK_NULL_HANDLE, &m_Samplers[Type]);
	}

	return m_Samplers[Type];
}