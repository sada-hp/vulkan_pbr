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
	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	createInfo.ppEnabledLayerNames = validationLayers.data();
	populateDebugMessengerCreateInfo(debugCreateInfo);
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = VK_NULL_HANDLE;
#endif
	return vkCreateInstance(&createInfo, VK_NULL_HANDLE, &instance) == VK_SUCCESS;
}

VkBool32 VulkanBase::create_swapchain_images()
{
	assert(Scope.GetSwapchain() != VK_NULL_HANDLE);

	uint32_t imagesCount = Scope.GetMaxFramesInFlight();
	swapchainImages.resize(imagesCount);
	depthAttachments.resize(imagesCount);
	hdrAttachments.resize(imagesCount);
	VkBool32 res = vkGetSwapchainImagesKHR(Scope.GetDevice(), Scope.GetSwapchain(), &imagesCount, swapchainImages.data()) == VK_SUCCESS;

	const TArray<uint32_t, 2> queueIndices = { Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex() };
	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		VkImageView imageView;

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = swapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.format = Scope.GetColorFormat();

		VkImageCreateInfo hdrInfo{};
		hdrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		hdrInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		hdrInfo.arrayLayers = 1;
		hdrInfo.extent = { Scope.GetSwapchainExtent().width, Scope.GetSwapchainExtent().height, 1 };
		hdrInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		hdrInfo.imageType = VK_IMAGE_TYPE_2D;
		hdrInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		hdrInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		hdrInfo.mipLevels = 1;
		hdrInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		hdrInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		hdrInfo.queueFamilyIndexCount = queueIndices.size();
		hdrInfo.pQueueFamilyIndices = queueIndices.data();

		VkImageCreateInfo depthInfo{};
		depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		depthInfo.format = Scope.GetDepthFormat();
		depthInfo.arrayLayers = 1;
		depthInfo.extent = { Scope.GetSwapchainExtent().width, Scope.GetSwapchainExtent().height, 1 };
		depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		depthInfo.imageType = VK_IMAGE_TYPE_2D;
		depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		depthInfo.mipLevels = 1;
		depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		depthInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		depthInfo.queueFamilyIndexCount = queueIndices.size();
		depthInfo.pQueueFamilyIndices = queueIndices.data();

		res = (vkCreateImageView(Scope.GetDevice(), &viewInfo, VK_NULL_HANDLE, &imageView) == VK_SUCCESS) & res;
		swapchainViews.push_back(imageView);

		viewInfo.format = hdrInfo.format;
		hdrAttachments[i] = std::make_unique<VulkanImage>(Scope);
		hdrAttachments[i]->CreateImage(hdrInfo, allocCreateInfo)
			.CreateImageView(viewInfo)
			.TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.format = Scope.GetDepthFormat();
		depthAttachments[i] = std::make_unique<VulkanImage>(Scope);
		depthAttachments[i]->CreateImage(depthInfo, allocCreateInfo)
			.CreateImageView(viewInfo);
	}

	return res;
}

VkBool32 VulkanBase::create_framebuffers()
{
	assert(swapchainImages.size() > 0 && Scope.GetRenderPass() != VK_NULL_HANDLE);

	framebuffers.resize(swapchainImages.size());

	VkBool32 res = 1;
	for (size_t i = 0; i < framebuffers.size(); i++)
	{
		res = CreateFramebuffer(Scope.GetDevice(), Scope.GetRenderPass(), Scope.GetSwapchainExtent(), { hdrAttachments[i]->GetImageView(), swapchainViews[i], depthAttachments[i]->GetImageView() }, &framebuffers[i]) & res;
	}

	return res;
}

VkBool32 VulkanBase::create_hdr_pipeline()
{
	VkBool32 res = 1;
	HDRPipelines.resize(swapchainImages.size());
	HDRDescriptors.resize(swapchainImages.size());

	for (size_t i = 0; i < HDRDescriptors.size(); i++)
	{
		HDRDescriptors[i] = DescriptorSetDescriptor()
			.AddSubpassAttachment(0, VK_SHADER_STAGE_FRAGMENT_BIT, *hdrAttachments[i])
			.Allocate(Scope);

		HDRPipelines[i] = GraphicsPipelineDescriptor()
			.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
			.SetShaderStage("hdr_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
			.AddDescriptorLayout(HDRDescriptors[i]->GetLayout())
			.SetSubpass(1)
			.Construct(Scope);
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
	uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	uboInfo.size = sizeof(UniformBuffer);
	uboInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo uboAllocCreateInfo{};
	uboAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	uboAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	ubo = std::make_unique<Buffer>(Scope, uboInfo, uboAllocCreateInfo);

	VkBufferCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	viewInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	viewInfo.size = sizeof(ViewBuffer);
	viewInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	view = std::make_unique<Buffer>(Scope, viewInfo, uboAllocCreateInfo);

	defaultWhite = std::shared_ptr<VulkanImage>(GRNoise::GenerateSolidColor(Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_SRGB, std::byte(255u), std::byte(255u), std::byte(255u), std::byte(255u)));
	defaultNormal = std::shared_ptr<VulkanImage>(GRNoise::GenerateSolidColor(Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(127u), std::byte(127u), std::byte(255u), std::byte(255u)));
	defaultBlack = std::shared_ptr<VulkanImage>(GRNoise::GenerateSolidColor(Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(0u)));

	return res;
}