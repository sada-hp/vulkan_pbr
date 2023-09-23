#include "pch.hpp"
#define GLFW_INCLUDE_VULKAN
#define VMA_IMPLEMENTATION
#define VK_KHR_swapchain
#define STB_IMAGE_IMPLEMENTATION
#include "renderer.hpp"

extern std::string exec_path;

VulkanBase::VulkanBase(GLFWwindow* window)
	: _glfwWindow(window)
{
	VkBool32 res = _initializeInstance();
	res = _enumeratePhysicalDevices() & res;
	res = (glfwCreateWindowSurface(_instance, _glfwWindow, VK_NULL_HANDLE, &_surface) == VK_SUCCESS) & res;
	res = _initializeLogicalDevice() & res;
	res = _initializeAllocator() & res;
	res = _initializeSwapchain() & res;
	res = _initializeSwapchainImages() & res;
	res = _initializeRenderPass() & res;
	res = _initializeFramebuffers() & res;
	res = _initializeCommandPool(_graphicsQueue.index, &_graphicsPool) & res;
	res = _initializeCommandPool(_transferQueue.index, &_transferPool) & res;

	_presentBuffers.resize(_swapchainImages.size());
	_presentSemaphores.resize(_swapchainImages.size());
	_presentFences.resize(_swapchainImages.size());
	res = _allocateBuffers(_graphicsPool, _presentBuffers.size(), _presentBuffers.data()) & res;

	std::for_each(_presentSemaphores.begin(), _presentSemaphores.end(), [&, this](VkSemaphore& it)
		{
			res = _createSemaphore(&it) & res;
		});

	res = _createSemaphore(&_swapchainSemaphore) & res;

	std::for_each(_presentFences.begin(), _presentFences.end(), [&, this](VkFence& it)
		{
			res = _createFence(&it) & res;
		});

	res = _createFence(&_graphicsFence) & res;
	res = _createFence(&_transferFence) & res;

	VkBufferCreateInfo uboInfo{};
	uboInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	uboInfo.size = sizeof(ModelViewProjection);
	uboInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo uboAllocCreateInfo{};
	uboAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	uboAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	res = _ubo.create(_allocator, &uboInfo, &uboAllocCreateInfo) & res;

	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 2;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 1;
	res = _initializeDescriptorPool(poolSizes.data(), poolSizes.size(), 2) & res;

	res = _prepareScene() & res;

	assert(res != 0);
}

VulkanBase::~VulkanBase() noexcept
{
	vkWaitForFences(_logicalDevice, _presentFences.size(), _presentFences.data(), VK_TRUE, UINT64_MAX);

	_ubo.destroy(_allocator);
	_skyboxImg.destroy(_logicalDevice, _allocator);
	triangle.destroy(_logicalDevice, _descriptorPool);
	skybox.destroy(_logicalDevice, _descriptorPool);

	vkDestroyDescriptorPool(_logicalDevice, _descriptorPool, VK_NULL_HANDLE);
	vkDestroyFence(_logicalDevice, _transferFence, VK_NULL_HANDLE);
	vkDestroyFence(_logicalDevice, _graphicsFence, VK_NULL_HANDLE);
	std::erase_if(_presentFences, [&, this](VkFence& it)
		{
			vkDestroyFence(_logicalDevice, it, VK_NULL_HANDLE);
			return true;
		});
	vkDestroySemaphore(_logicalDevice, _swapchainSemaphore, VK_NULL_HANDLE);
	std::erase_if(_presentSemaphores, [&, this](VkSemaphore& it)
		{
			vkDestroySemaphore(_logicalDevice, it, VK_NULL_HANDLE);
			return true;
		});
	vkFreeCommandBuffers(_logicalDevice, _graphicsPool, _presentBuffers.size(), _presentBuffers.data());
	vkDestroyCommandPool(_logicalDevice, _transferPool, VK_NULL_HANDLE);
	vkDestroyCommandPool(_logicalDevice, _graphicsPool, VK_NULL_HANDLE);
	std::erase_if(_framebuffers, [&, this](VkFramebuffer& fb)
		{
			vkDestroyFramebuffer(_logicalDevice, fb, VK_NULL_HANDLE);
			return true;
		});
	vkDestroyRenderPass(_logicalDevice, _renderPass, VK_NULL_HANDLE);
	std::erase_if(_swapchainViews, [&, this](VkImageView view)
		{
			vkDestroyImageView(_logicalDevice, view, VK_NULL_HANDLE);
			return true;
		});
	std::erase_if(_depthAttachments, [&, this](vkImageStruct img)
		{
			img.destroy(_logicalDevice, _allocator);
			return true;
		});
	vkDestroySwapchainKHR(_logicalDevice, _swapchain, VK_NULL_HANDLE);
	vmaDestroyAllocator(_allocator);
	vkDestroyDevice(_logicalDevice, VK_NULL_HANDLE);
	vkDestroySurfaceKHR(_instance, _surface, VK_NULL_HANDLE);
	vkDestroyInstance(_instance, VK_NULL_HANDLE);
}

void VulkanBase::Step() const
{
	if (_swapchainExtent.width == 0 || _swapchainExtent.height == 0)
		return;

	uint32_t image_index = 0;
	vkAcquireNextImageKHR(_logicalDevice, _swapchain, UINT64_MAX, _swapchainSemaphore, VK_NULL_HANDLE, &image_index);

	vkWaitForFences(_logicalDevice, 1, &_presentFences[image_index], VK_TRUE, UINT64_MAX);
	vkResetFences(_logicalDevice, 1, &_presentFences[image_index]);

	memcpy(_ubo.allocInfo.pMappedData, &camera.get_mvp(), sizeof(ModelViewProjection));
	vmaFlushAllocation(_allocator, _ubo.memory, 0, VK_WHOLE_SIZE); //clean cache


	//////////////////////////////////////////////////////////
	{
		VkResult res;
		const VkCommandBuffer& cmd = _presentBuffers[image_index];
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		res = vkBeginCommandBuffer(cmd, &beginInfo);

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = _framebuffers[image_index];
		renderPassInfo.renderPass = _renderPass;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = _swapchainExtent;
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues;

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)_swapchainExtent.width;
		viewport.height = (float)_swapchainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = _swapchainExtent;
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox.pipelineLayout, 0, 1, &skybox.descriptorSet, 0, VK_NULL_HANDLE);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox.pipeline);
		vkCmdDraw(cmd, 36, 1, 0, 0);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle.pipelineLayout, 0, 1, &triangle.descriptorSet, 0, VK_NULL_HANDLE);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle.pipeline);
		vkCmdDraw(cmd, 3, 1, 0, 0);
		vkCmdEndRenderPass(cmd);

		vkEndCommandBuffer(cmd);
	}
	//////////////////////////////////////////////////////////

	VkSubmitInfo submitInfo{};
	std::array<VkSemaphore, 1> waitSemaphores = { _swapchainSemaphore };
	std::array<VkSemaphore, 1> signalSemaphores = { _presentSemaphores[image_index] };

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = waitSemaphores.size();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &_presentBuffers[image_index];
	submitInfo.signalSemaphoreCount = signalSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	VkResult res = vkQueueSubmit(_graphicsQueue.queue, 1, &submitInfo, _presentFences[image_index]);

	assert(res != VK_ERROR_DEVICE_LOST);

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = signalSemaphores.size();
	presentInfo.pWaitSemaphores = signalSemaphores.data();
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.pImageIndices = &image_index;
	presentInfo.pResults = VK_NULL_HANDLE;

	vkQueuePresentKHR(_graphicsQueue.queue, &presentInfo);
}

void VulkanBase::HandleResize()
{
	vkWaitForFences(_logicalDevice, _presentFences.size(), _presentFences.data(), VK_TRUE, UINT64_MAX);

	std::erase_if(_framebuffers, [&, this](VkFramebuffer& fb)
		{
			vkDestroyFramebuffer(_logicalDevice, fb, VK_NULL_HANDLE);
			return true;
		});
	std::erase_if(_depthAttachments, [&, this](vkImageStruct img)
		{
			img.destroy(_logicalDevice, _allocator);
			return true;
		});
	std::erase_if(_swapchainViews, [&, this](VkImageView view)
		{
			vkDestroyImageView(_logicalDevice, view, VK_NULL_HANDLE);
			return true;
		});
	_swapchainImages.resize(0);
	vkDestroySwapchainKHR(_logicalDevice, _swapchain, VK_NULL_HANDLE);
	_swapchain = VK_NULL_HANDLE;

	if (_initializeSwapchain())
	{
		_initializeSwapchainImages();
		_initializeFramebuffers();

		//it shouldn't change afaik, but better to keep it under control
		assert(_swapchainImages.size() == _presentBuffers.size());
	}
}

VkBool32 VulkanBase::_initializeInstance()
{
#ifdef _VkValidation
	assert(checkValidationLayerSupport());
#endif

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "AVR_App";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
	appInfo.pEngineName = "AVR";
	appInfo.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	const auto& ext = _getRequiredExtensions();

	createInfo.enabledExtensionCount = ext.size();
	createInfo.ppEnabledExtensionNames = ext.data();

#ifdef _VkValidation
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	createInfo.ppEnabledLayerNames = validationLayers.data();
	populateDebugMessengerCreateInfo(debugCreateInfo);
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = VK_NULL_HANDLE;
#endif
	return vkCreateInstance(&createInfo, VK_NULL_HANDLE, &_instance) == VK_SUCCESS;
}

VkBool32 VulkanBase::_enumeratePhysicalDevices()
{
	uint32_t devicesCount;
	std::vector<VkPhysicalDevice> devicesArray;

	vkEnumeratePhysicalDevices(_instance, &devicesCount, VK_NULL_HANDLE);
	devicesArray.resize(devicesCount);
	vkEnumeratePhysicalDevices(_instance, &devicesCount, devicesArray.data());

	const auto& it = std::find_if(devicesArray.cbegin(), devicesArray.cend(), [&, this](const VkPhysicalDevice& itt)
		{
			return _enumerateDeviceExtensions(itt, _extensions);
		});

	if (it != devicesArray.end())
	{
		_physicalDevice = *it;
		return VK_TRUE;
	}

	return VK_FALSE;
}

VkBool32 VulkanBase::_initializeLogicalDevice()
{
	assert(_physicalDevice != VK_NULL_HANDLE);

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.imageCubeArray = VK_TRUE;
	deviceFeatures.fullDrawIndexUint32 = VK_TRUE;
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	std::vector<uint32_t> queueFamilies(_findQueues({ VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_COMPUTE_BIT }));

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : queueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.ppEnabledExtensionNames = _extensions.data();
	createInfo.enabledExtensionCount = _extensions.size();
	createInfo.pQueueCreateInfos = queueInfos.data();
	createInfo.queueCreateInfoCount = queueInfos.size();

	VkBool32 res = vkCreateDevice(_physicalDevice, &createInfo, VK_NULL_HANDLE, &_logicalDevice) == VK_SUCCESS;

	_graphicsQueue = _getVkQueueStruct(queueFamilies[0]);
	_transferQueue = _getVkQueueStruct(queueFamilies[1]);
	_computeQueue = _getVkQueueStruct(queueFamilies[2]);

	return res;
}

VkBool32 VulkanBase::_initializeAllocator()
{
	assert(_logicalDevice != VK_NULL_HANDLE && _instance != VK_NULL_HANDLE && _physicalDevice != VK_NULL_HANDLE);

	VmaAllocatorCreateInfo createInfo{};
	createInfo.device = _logicalDevice;
	createInfo.instance = _instance;
	createInfo.physicalDevice = _physicalDevice;
	createInfo.vulkanApiVersion = VK_API_VERSION_1_2;

	return vmaCreateAllocator(&createInfo, &_allocator) == VK_SUCCESS;
}

VkBool32 VulkanBase::_initializeSwapchain()
{
	assert(_surface != VK_NULL_HANDLE);

	VkSwapchainCreateInfoKHR createInfo{};
	VkSurfaceCapabilitiesKHR capabilities{};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;

	uint32_t temp = 0;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &temp, VK_NULL_HANDLE);
	formats.resize(temp);
	vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &temp, formats.data());

	vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &temp, VK_NULL_HANDLE);
	presentModes.resize(temp);
	vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &temp, presentModes.data());

	const auto& surfaceFormat = std::find_if(formats.cbegin(), formats.cend(), [](const VkSurfaceFormatKHR& it)
		{
			return it.format == VK_FORMAT_B8G8R8A8_SRGB && it.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		});

	assert(surfaceFormat != formats.cend());

	int fbWidth = 0, fbHeight = 0;
	glfwGetFramebufferSize(_glfwWindow, &fbWidth, &fbHeight);

	std::vector<uint32_t> queueFamilies(_findQueues({ VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT }));
	uint32_t desiredImageCount = glm::min(capabilities.minImageCount + 1, capabilities.maxImageCount);

	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.minImageCount = desiredImageCount;
	createInfo.surface = _surface;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageArrayLayers = 1;
	createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.clipped = VK_TRUE;
	createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	createInfo.imageFormat = surfaceFormat->format;
	createInfo.imageColorSpace = surfaceFormat->colorSpace;
	createInfo.imageExtent.width = std::clamp(static_cast<uint32_t>(fbWidth), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	createInfo.imageExtent.height = std::clamp(static_cast<uint32_t>(fbHeight), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	createInfo.queueFamilyIndexCount = queueFamilies.size();
	createInfo.pQueueFamilyIndices = queueFamilies.data();

	_swapchainExtent = createInfo.imageExtent;
	_swapchainFormat = createInfo.imageFormat;

	if (_swapchainExtent.width == 0 || _swapchainExtent.height == 0)
		return VK_FALSE;

	glm::mat4 projection = glm::perspective(30.f, static_cast<float>(_swapchainExtent.width) / static_cast<float>(_swapchainExtent.height), 0.1f, 1000.f);
	projection[1][1] *= -1;
	camera._set_projection(projection);

	return vkCreateSwapchainKHR(_logicalDevice, &createInfo, VK_NULL_HANDLE, &_swapchain) == VK_SUCCESS;
}

VkBool32 VulkanBase::_initializeSwapchainImages()
{
	assert(_swapchain != VK_NULL_HANDLE);

	uint32_t imagesCount = 0;
	vkGetSwapchainImagesKHR(_logicalDevice, _swapchain, &imagesCount, VK_NULL_HANDLE);
	_swapchainImages.resize(imagesCount);
	_depthAttachments.resize(imagesCount);
	VkBool32 res = vkGetSwapchainImagesKHR(_logicalDevice, _swapchain, &imagesCount, _swapchainImages.data()) == VK_SUCCESS;

	for (VkImage image : _swapchainImages)
	{
		VkImageView imageView;
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.format = _swapchainFormat;

		res = (vkCreateImageView(_logicalDevice, &viewInfo, VK_NULL_HANDLE, &imageView) == VK_SUCCESS) & res;
		_swapchainViews.push_back(imageView);
	}

	const std::array<uint32_t, 2> queueIndices = { _graphicsQueue.index, _transferQueue.index };
	VmaAllocationCreateInfo allocCreateInfo{};
	VkImageCreateInfo depthInfo{};
	depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthInfo.format = _depthFormat;
	depthInfo.arrayLayers = 1;
	depthInfo.extent = { _swapchainExtent.width, _swapchainExtent.height, 1 };
	depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthInfo.imageType = VK_IMAGE_TYPE_2D;
	depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthInfo.mipLevels = 1;
	depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
	depthInfo.queueFamilyIndexCount = queueIndices.size();
	depthInfo.pQueueFamilyIndices = queueIndices.data();

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.format = _depthFormat;

	for (auto& attachment : _depthAttachments)
	{
		res = attachment.create(_logicalDevice, _allocator, &depthInfo, &viewInfo, VK_NULL_HANDLE, &allocCreateInfo) & res;
	}

	return res;
}

VkBool32 VulkanBase::_initializeRenderPass()
{
	assert(_swapchainImages.size() > 0);

	VkRenderPassCreateInfo createInfo{};
	std::array<VkAttachmentDescription, 2> attachments;

	//Color attachment
	attachments[0].format = _swapchainFormat;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].flags = 0;
	// Depth attachment
	attachments[1].format = _depthFormat;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].flags = 0;

	VkAttachmentReference color_ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depth_ref{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpassDescription{};

	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &color_ref;
	subpassDescription.pDepthStencilAttachment = &depth_ref;
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = attachments.size();
	createInfo.pAttachments = attachments.data();
	createInfo.dependencyCount = 0;
	createInfo.pDependencies = VK_NULL_HANDLE;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpassDescription;


	return vkCreateRenderPass(_logicalDevice, &createInfo, VK_NULL_HANDLE, &_renderPass) == VK_SUCCESS;
}

VkBool32 VulkanBase::_initializeFramebuffers()
{
	assert(_swapchainImages.size() > 0 && _renderPass != VK_NULL_HANDLE);

	_framebuffers.resize(_swapchainImages.size());

	VkBool32 res = 1;
	for (size_t i = 0; i < _framebuffers.size(); i++)
	{
		std::array<VkImageView, 2> attachments = { _swapchainViews[i], _depthAttachments[i].view };
		VkFramebufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.attachmentCount = attachments.size();
		createInfo.pAttachments = attachments.data();
		createInfo.layers = 1;
		createInfo.width = _swapchainExtent.width;
		createInfo.height = _swapchainExtent.height;
		createInfo.renderPass = _renderPass;
		res = (vkCreateFramebuffer(_logicalDevice, &createInfo, VK_NULL_HANDLE, &_framebuffers[i]) == VK_SUCCESS) & res;
	}

	return res;
}

VkBool32 VulkanBase::_initializeCommandPool(const uint32_t targetQueueIndex, VkCommandPool* outPool)
{
	VkCommandPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = targetQueueIndex;
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	return vkCreateCommandPool(_logicalDevice, &createInfo, VK_NULL_HANDLE, outPool) == VK_SUCCESS;
}

VkBool32 VulkanBase::_initializeDescriptorPool(const VkDescriptorPoolSize* poolSizes, const size_t poolSizesCount, const uint32_t setsCount)
{
	VkDescriptorPoolCreateInfo dpoolInfo{};
	dpoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpoolInfo.maxSets = setsCount;
	dpoolInfo.poolSizeCount = poolSizesCount;
	dpoolInfo.pPoolSizes = poolSizes;
	dpoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	return vkCreateDescriptorPool(_logicalDevice, &dpoolInfo, VK_NULL_HANDLE, &_descriptorPool) == VK_SUCCESS;
}

VkBool32 VulkanBase::_allocateBuffers(const VkCommandPool& pool, const uint32_t count, VkCommandBuffer* outBuffers)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandBufferCount = count;
	allocInfo.commandPool = pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	return vkAllocateCommandBuffers(_logicalDevice, &allocInfo, outBuffers) == VK_SUCCESS;
}

VkBool32 VulkanBase::_createFence(VkFence* outFence)
{
	VkFenceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	return vkCreateFence(_logicalDevice, &createInfo, VK_NULL_HANDLE, outFence) == VK_SUCCESS;
}

VkBool32 VulkanBase::_createSemaphore(VkSemaphore* outSemaphore)
{
	VkSemaphoreCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	return vkCreateSemaphore(_logicalDevice, &createInfo, VK_NULL_HANDLE, outSemaphore) == VK_SUCCESS;
}

VkBool32 VulkanBase::_enumerateDeviceExtensions(const VkPhysicalDevice& physicalDevice, const std::vector<const char*>& desired_extensions) const
{
	uint32_t extensionCount;

	vkEnumerateDeviceExtensionProperties(physicalDevice, VK_NULL_HANDLE, &extensionCount, VK_NULL_HANDLE);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, VK_NULL_HANDLE, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(desired_extensions.begin(), desired_extensions.end());
	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

std::vector<uint32_t> VulkanBase::_findQueues(const std::vector<VkQueueFlagBits>& flags) const
{
	uint32_t familiesCount;
	std::vector<uint32_t> output(flags.size());
	std::vector<VkQueueFamilyProperties> queueFamilies;

	vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &familiesCount, VK_NULL_HANDLE);
	queueFamilies.resize(familiesCount);
	vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &familiesCount, queueFamilies.data());

	//reverse to try to find a flag specific queue first and general queue if it doesn't exist
	std::reverse(queueFamilies.begin(), queueFamilies.end());

	int j = 0;
	for (const auto& flag : flags)
	{
		int i = queueFamilies.size() - 1;
		for (const auto& family : queueFamilies)
		{
			if ((flag & family.queueFlags) != 0)
			{
				output[j++] = i;
				break;
			}

			i--;
		}

		assert(i >= 0);
	}

	return output;
}

vkQueueStruct VulkanBase::_getVkQueueStruct(const uint32_t& familyIndex) const
{
	assert(_logicalDevice != VK_NULL_HANDLE);

	vkQueueStruct output{};
	output.index = familyIndex;
	vkGetDeviceQueue(_logicalDevice, familyIndex, 0, &output.queue);

	assert(output.queue != VK_NULL_HANDLE);

	return output;
}

void VulkanBase::_generate_mip_maps(VkImage image, VkExtent2D dimensions, VkImageSubresourceRange subRange)
{
	if (subRange.levelCount == 1)
		return;

	VkCommandBuffer commandBuffer;
	VkCommandBufferAllocateInfo cmdAlloc{};
	cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAlloc.commandBufferCount = 1;
	cmdAlloc.commandPool = _graphicsPool;
	cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	vkAllocateCommandBuffers(_logicalDevice, &cmdAlloc, &commandBuffer);

	VkFence mipFence;
	_createFence(&mipFence);

	VkCommandBufferBeginInfo cmdBegin{};
	cmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &cmdBegin);

	for (int k = 0; k < subRange.layerCount; k++)
	{
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = subRange.aspectMask;
		barrier.subresourceRange.baseArrayLayer = k;
		barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;

		int32_t mipWidth = dimensions.width;
		int32_t mipHeight = dimensions.height;

		for (uint32_t i = 1; i < subRange.levelCount; i++)
		{
			barrier.subresourceRange.baseMipLevel = i;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = k;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = k;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
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

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	}

	VkSubmitInfo submition{};
	submition.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submition.commandBufferCount = 1;
	submition.pCommandBuffers = &commandBuffer;
	vkEndCommandBuffer(commandBuffer);
	vkQueueSubmit(_graphicsQueue.queue, 1, &submition, mipFence);
	vkWaitForFences(_logicalDevice, 1, &mipFence, VK_TRUE, UINT64_MAX);
	vkFreeCommandBuffers(_logicalDevice, _graphicsPool, 1, &commandBuffer);
	vkDestroyFence(_logicalDevice, mipFence, VK_NULL_HANDLE);
}

VkBool32 VulkanBase::_prepareScene()
{
	VkBool32 res = 1;

	{
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.dstBinding = 0;
		write.pBufferInfo = &_ubo.descriptorInfo;
		VkDescriptorSetLayoutBinding binding{};
		binding.binding = 0;
		binding.descriptorCount = 1;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		vkShaderPipelineCreateInfo triCI{};
		triCI.bindingsCount = 1;
		triCI.pBindings = &binding;
		triCI.descriptorPool = _descriptorPool;
		triCI.renderPass = _renderPass;
		triCI.shaderStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		triCI.shaderName = exec_path + "shaders\\default";
		triCI.writesCount = 1;
		triCI.pWrites = &write;

		res = triangle.create(_logicalDevice, triCI) & res;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////cube_texture

	{
		std::array<std::future<void>, 6> loaders;
		const std::array<std::string, 6> sky_collection = { (exec_path + "content\\Background_East.jpg"), (exec_path + "content\\Background_West.jpg"),
			(exec_path + "content\\Background_Top.jpg"), (exec_path + "content\\Background_Bottom.jpg"),
			(exec_path + "content\\Background_North.jpg"), (exec_path + "content\\Background_South.jpg") };
		constexpr int resolution = 2048 * 2048 * 4;
		unsigned char* pixels = (unsigned char*)malloc(resolution * 6);

		for (int i = 0; i < 6; i++)
		{
			loaders[i] = std::async(std::launch::async, [&, this, i]()
				{
					int x, y, c;
					void* target = stbi_load(sky_collection[i].c_str(), &x, &y, &c, 4);
					memcpy(pixels + resolution * i, target, resolution);
					free(target);
				});
		}

		std::for_each(loaders.begin(), loaders.end(), [](const auto& it) {it.wait(); });

		vkBufferStruct stagingBuffer{};
		VkBufferCreateInfo sbInfo{};
		sbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		sbInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		sbInfo.queueFamilyIndexCount = 1;
		sbInfo.pQueueFamilyIndices = &_transferQueue.index;
		sbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		sbInfo.size = resolution * 6;
		VmaAllocationCreateInfo sbAlloc{};
		sbAlloc.usage = VMA_MEMORY_USAGE_AUTO;
		sbAlloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		stagingBuffer.create(_allocator, &sbInfo, &sbAlloc);

		memcpy(stagingBuffer.allocInfo.pMappedData, pixels, resolution * 6);
		vmaFlushAllocation(_allocator, stagingBuffer.memory, 0, VK_WHOLE_SIZE);

		uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(2048, 2048)))) + 1;
		VkImageSubresourceRange skySubRes{};
		skySubRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		skySubRes.baseArrayLayer = 0;
		skySubRes.baseMipLevel = 0;
		skySubRes.layerCount = 6;
		skySubRes.levelCount = mipLevels;
		VkImageCreateInfo skyInfo{};
		skyInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		skyInfo.arrayLayers = 6;
		skyInfo.extent = { 2048, 2048, 1 };
		skyInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		skyInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		skyInfo.mipLevels = mipLevels;
		skyInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		skyInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		skyInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		skyInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		skyInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		skyInfo.imageType = VK_IMAGE_TYPE_2D;
		VkImageViewCreateInfo skyView{};
		skyView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		skyView.format = VK_FORMAT_R8G8B8A8_SRGB;
		skyView.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		skyView.subresourceRange = skySubRes;
		VmaAllocationCreateInfo skyAlloc{};
		skyAlloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(_physicalDevice, &properties);
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1;
		_skyboxImg.create(_logicalDevice, _allocator, &skyInfo, &skyView, &samplerInfo, &skyAlloc);
		VkCommandBuffer cmd;
		VkCommandBufferAllocateInfo cmdAlloc{};
		cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAlloc.commandBufferCount = 1;
		cmdAlloc.commandPool = _transferPool;
		cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		VkCommandBufferBeginInfo cmdBegin{};
		cmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkAllocateCommandBuffers(_logicalDevice, &cmdAlloc, &cmd);
		vkBeginCommandBuffer(cmd, &cmdBegin);

		VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = _skyboxImg.image;
		barrier.subresourceRange = skySubRes;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

		int i = 0;
		std::array<VkBufferImageCopy, 6> regions = {};
		for (auto& region : regions)
		{
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.baseArrayLayer = i;
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.mipLevel = 0;
			region.bufferOffset = i * resolution;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { 2048, 2048, 1 };
			i++;
		}

		vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, _skyboxImg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size(), regions.data());
		VkSubmitInfo submition{};
		submition.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submition.commandBufferCount = 1;
		submition.pCommandBuffers = &cmd;

		vkEndCommandBuffer(cmd);
		vkQueueSubmit(_transferQueue.queue, 1, &submition, _transferFence);
		vkWaitForFences(_logicalDevice, 1, &_transferFence, VK_TRUE, UINT64_MAX);
		vkFreeCommandBuffers(_logicalDevice, _transferPool, 1, &cmd);

		cmdAlloc.commandPool = _graphicsPool;
		vkAllocateCommandBuffers(_logicalDevice, &cmdAlloc, &cmd);
		vkBeginCommandBuffer(cmd, &cmdBegin);
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
		vkEndCommandBuffer(cmd);
		vkQueueSubmit(_graphicsQueue.queue, 1, &submition, _graphicsFence);
		vkWaitForFences(_logicalDevice, 1, &_graphicsFence, VK_TRUE, UINT64_MAX);
		vkFreeCommandBuffers(_logicalDevice, _graphicsPool, 1, &cmd);

		_generate_mip_maps(_skyboxImg.image, { 2048, 2048 }, skySubRes);

		stagingBuffer.destroy(_allocator);
		free(pixels);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////skybox

	{
		std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		std::array<VkWriteDescriptorSet, 2> writes{};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].dstBinding = 0;
		writes[0].pBufferInfo = &_ubo.descriptorInfo;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].dstBinding = 1;
		writes[1].pImageInfo = &_skyboxImg.descriptorInfo;
		
		vkShaderPipelineCreateInfo skyboxCI{};
		skyboxCI.bindingsCount = bindings.size();
		skyboxCI.pBindings = bindings.data();
		skyboxCI.writesCount = writes.size();
		skyboxCI.pWrites = writes.data();
		skyboxCI.renderPass = _renderPass;
		skyboxCI.descriptorPool = _descriptorPool;
		skyboxCI.shaderName = exec_path + "shaders/background";
		skyboxCI.shaderStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		res = skybox.create(_logicalDevice, skyboxCI) & res;
	}

	return res;
}

std::vector<const char*> VulkanBase::_getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef _VkValidation
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	return extensions;
}

#ifdef _VkValidation
VkResult VulkanBase::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) 
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != VK_NULL_HANDLE) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanBase::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != VK_NULL_HANDLE) {
		func(instance, debugMessenger, pAllocator);
	}
}

void VulkanBase::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}

void VulkanBase::setupDebugMessenger() 
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(_instance, &createInfo, VK_NULL_HANDLE, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

VkBool32 VulkanBase::checkValidationLayerSupport() const
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, VK_NULL_HANDLE);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) 
	{
		VkBool32 layerFound = false;

		for (const auto& layerProperties : availableLayers) 
		{
			if (strcmp(layerName, layerProperties.layerName) == 0) 
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

VkBool32 VulkanBase::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
{
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}
#endif