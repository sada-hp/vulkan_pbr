#include "pch.hpp"
#include "renderer.hpp"

TVector<const char*> VulkanBase::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	TVector<const char*> rqextensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef VALIDATION
	rqextensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	return rqextensions;
}

VkBool32 VulkanBase::create_instance()
{
#ifdef VALIDATION
	assert(checkValidationLayerSupport());
#endif

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "AVR_App";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
	appInfo.pEngineName = "AVR";
	appInfo.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	const auto& ext = getRequiredExtensions();

	createInfo.enabledExtensionCount = ext.size();
	createInfo.ppEnabledExtensionNames = ext.data();

#ifdef VALIDATION
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
	createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
	populateDebugMessengerCreateInfo(debugCreateInfo);
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = VK_NULL_HANDLE;
#endif
	return vkCreateInstance(&createInfo, VK_NULL_HANDLE, &m_VkInstance) == VK_SUCCESS;
}

VkBool32 VulkanBase::create_swapchain_images()
{
	assert(m_Scope.GetSwapchain() != VK_NULL_HANDLE);

	uint32_t imagesCount = m_Scope.GetMaxFramesInFlight();
	m_SwapchainImages.resize(imagesCount);
	m_SwapchainViews.resize(imagesCount);

	m_DepthAttachmentsHR.resize(imagesCount);
	m_HdrAttachmentsHR.resize(imagesCount);
	m_HdrViewsHR.resize(imagesCount);
	m_DepthViewsHR.resize(imagesCount);

	m_DepthAttachmentsLR.resize(imagesCount);
	m_HdrAttachmentsLR.resize(imagesCount);
	m_HdrViewsLR.resize(imagesCount);
	m_DepthViewsLR.resize(imagesCount);

	VkBool32 res = vkGetSwapchainImagesKHR(m_Scope.GetDevice(), m_Scope.GetSwapchain(), &imagesCount, m_SwapchainImages.data()) == VK_SUCCESS;

	const TArray<uint32_t, 2> queueIndices = { m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex() };
	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	for (size_t i = 0; i < m_SwapchainImages.size(); ++i)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_SwapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.format = m_Scope.GetColorFormat();

		VkImageCreateInfo hdrInfo{};
		hdrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		hdrInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		hdrInfo.arrayLayers = 1;
		hdrInfo.extent = { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 };
		hdrInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		hdrInfo.imageType = VK_IMAGE_TYPE_2D;
		hdrInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		hdrInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		hdrInfo.mipLevels = 1;
		hdrInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		hdrInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		hdrInfo.queueFamilyIndexCount = queueIndices.size();
		hdrInfo.pQueueFamilyIndices = queueIndices.data();

		VkImageCreateInfo depthInfo{};
		depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		depthInfo.format = m_Scope.GetDepthFormat();
		depthInfo.arrayLayers = 1;
		depthInfo.extent = { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 };
		depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		depthInfo.imageType = VK_IMAGE_TYPE_2D;
		depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		depthInfo.mipLevels = 1;
		depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		depthInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		depthInfo.queueFamilyIndexCount = queueIndices.size();
		depthInfo.pQueueFamilyIndices = queueIndices.data();

		res = (vkCreateImageView(m_Scope.GetDevice(), &viewInfo, VK_NULL_HANDLE, &m_SwapchainViews[i]) == VK_SUCCESS) & res;

		m_HdrAttachmentsHR[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_HdrViewsHR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_HdrAttachmentsHR[i]);

		m_DepthAttachmentsHR[i] = std::make_unique<VulkanImage>(m_Scope, depthInfo, allocCreateInfo);
		m_DepthViewsHR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DepthAttachmentsHR[i]);

		hdrInfo.extent.width /= 2;
		hdrInfo.extent.height /= 2;
		hdrInfo.usage &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		hdrInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		m_HdrAttachmentsLR[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_HdrViewsLR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_HdrAttachmentsLR[i]);

		depthInfo.extent.width /= 2;
		depthInfo.extent.height /= 2;
		m_DepthAttachmentsLR[i] = std::make_unique<VulkanImage>(m_Scope, depthInfo, allocCreateInfo);
		m_DepthViewsLR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DepthAttachmentsLR[i]);
	}

	return res;
}

VkBool32 VulkanBase::create_framebuffers()
{
	assert(m_SwapchainImages.size() > 0 && m_Scope.GetRenderPass() != VK_NULL_HANDLE);

	m_FramebuffersHR.resize(m_SwapchainImages.size());
	m_FramebuffersLR.resize(m_SwapchainImages.size());

	VkBool32 res = 1;
	for (size_t i = 0; i < m_SwapchainImages.size(); ++i)
	{
		res = CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetRenderPass(), m_Scope.GetSwapchainExtent(), { m_HdrViewsHR[i]->GetImageView(), m_SwapchainViews[i], m_DepthViewsHR[i]->GetImageView() }, &m_FramebuffersHR[i]) & res;
		res = CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetLowResRenderPass(), { m_Scope.GetSwapchainExtent().width / 2, m_Scope.GetSwapchainExtent().height / 2 }, { m_HdrViewsLR[i]->GetImageView(), m_DepthViewsLR[i]->GetImageView() }, &m_FramebuffersLR[i]) & res;
	}

	return res;
}

VkBool32 VulkanBase::create_hdr_pipeline()
{
	VkBool32 res = 1;
	m_HDRPipelines.resize(m_SwapchainImages.size());
	m_HDRDescriptors.resize(m_SwapchainImages.size());

	for (size_t i = 0; i < m_HDRDescriptors.size(); ++i)
	{
		m_HDRDescriptors[i] = DescriptorSetDescriptor()
			.AddSubpassAttachment(0, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsHR[i]->GetImageView())
			.Allocate(m_Scope);

		m_HDRPipelines[i] = GraphicsPipelineDescriptor()
			.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
			.SetShaderStage("hdr_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
			.AddDescriptorLayout(m_HDRDescriptors[i]->GetLayout())
			.SetSubpass(1)
			.Construct(m_Scope);
	}

	return res;
}

VkBool32 VulkanBase::prepare_renderer_resources()
{
	VkBool32 res = 1;
	auto vertAttributes = Vertex::getAttributeDescriptions();
	auto vertBindings = Vertex::getBindingDescription();

	VkBufferCreateInfo uboInfo{};
	uboInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	uboInfo.size = sizeof(UniformBuffer);
	uboInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo uboAllocCreateInfo{};
	uboAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	uboAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	m_UBOBuffers.resize(m_SwapchainImages.size());
	m_UBOSets.resize(m_SwapchainImages.size());
	for (uint32_t i = 0; i < m_UBOBuffers.size(); ++i)
	{
		m_UBOBuffers[i] = std::make_unique<Buffer>(m_Scope, uboInfo, uboAllocCreateInfo);

		m_UBOSets[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_ALL, *m_UBOBuffers[i])
			.Allocate(m_Scope);
	}

	m_DefaultWhite = std::make_shared<VulkanTexture>();
	m_DefaultWhite->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_SRGB, std::byte(255u), std::byte(255u), std::byte(255u), std::byte(255u));
	m_DefaultWhite->View = std::make_unique<VulkanImageView>(m_Scope, *m_DefaultWhite->Image);

	m_DefaultBlack = std::make_shared<VulkanTexture>();
	m_DefaultBlack->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(0u));
	m_DefaultBlack->View = std::make_unique<VulkanImageView>(m_Scope, *m_DefaultBlack->Image);

	m_DefaultNormal = std::make_shared<VulkanTexture>();
	m_DefaultNormal->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(127u), std::byte(127u), std::byte(255u), std::byte(255u));
	m_DefaultNormal->View = std::make_unique<VulkanImageView>(m_Scope, *m_DefaultNormal->Image);

	m_DefaultARM = std::make_shared<VulkanTexture>();
	m_DefaultARM->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_SRGB, std::byte(255u), std::byte(255u), std::byte(0u), std::byte(255u));
	m_DefaultARM->View = std::make_unique<VulkanImageView>(m_Scope, *m_DefaultARM->Image);

	return res;
}