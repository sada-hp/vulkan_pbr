#pragma once
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <vma/vk_mem_alloc.h>
#include <vector>
#include <array>
#include <string>
#include <set>
#include <assert.h>
#include <iostream>

#define _VkValidation

class VulkanBase
{
	struct vkQueueStruct
	{
		VkQueue queue = VK_NULL_HANDLE;
		uint32_t index = 0;
	};

	struct vkImageStruct
	{
		void destroy(VkDevice device, VmaAllocator allocator)
		{
			vkDestroyImageView(device, view, VK_NULL_HANDLE);
			vmaDestroyImage(allocator, image, memory);
		}

		VkImage image = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		VmaAllocation memory = VK_NULL_HANDLE;
	};

	std::vector<const char*> _extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	std::vector<VkImage> _swapchainImages = {};
	std::vector<VkImageView> _swapchainViews = {};
	std::vector<vkImageStruct> _depthAttachments = {};
	std::vector<VkFramebuffer> _framebuffers = {};
	std::vector<VkFence> _presentFences = {};
	std::vector<VkSemaphore> _presentSemaphores = {};
	std::vector<VkCommandBuffer> _presentBuffers = {};

	vkQueueStruct _graphicsQueue = {};
	vkQueueStruct _transferQueue = {};
	vkQueueStruct _computeQueue = {};

	VkInstance _instance = VK_NULL_HANDLE;
	VkDevice _logicalDevice = VK_NULL_HANDLE;
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	VkSurfaceKHR _surface = VK_NULL_HANDLE;
	VmaAllocator _allocator = VK_NULL_HANDLE;
	VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
	VkSemaphore _swapchainSemaphore = VK_NULL_HANDLE;
	VkFence _graphicsFence = VK_NULL_HANDLE;
	VkFence _transferFence = VK_NULL_HANDLE;
	VkRenderPass _renderPass = VK_NULL_HANDLE;
	VkCommandPool _graphicsPool = VK_NULL_HANDLE;
	VkCommandPool _transferPool = VK_NULL_HANDLE;

	const VkFormat _depthFormat = VK_FORMAT_D16_UNORM;
	VkFormat _swapchainFormat;
	VkExtent2D _swapchainExtent = { 0, 0 };

	GLFWwindow* _glfwWindow = VK_NULL_HANDLE;

public:

	VulkanBase(GLFWwindow* window)
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

		assert(res != 0);
	}

	~VulkanBase() noexcept
	{
		vkWaitForFences(_logicalDevice, _presentFences.size(), _presentFences.data(), VK_TRUE, UINT64_MAX);

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

	void Step() const
	{
		if (_swapchainExtent.width == 0 || _swapchainExtent.height == 0)
			return;

		uint32_t image_index = 0;
		vkAcquireNextImageKHR(_logicalDevice, _swapchain, UINT64_MAX, _swapchainSemaphore, VK_NULL_HANDLE, &image_index);

		vkWaitForFences(_logicalDevice, 1, &_presentFences[image_index], VK_TRUE, UINT64_MAX);
		vkResetFences(_logicalDevice, 1, &_presentFences[image_index]);

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
			//vkCmdDraw(cmd, 3, 1, 0, 0);
			vkCmdEndRenderPass(cmd);

			vkEndCommandBuffer(cmd);
		}
		//////////////////////////////////////////////////////////

		VkSubmitInfo submitInfo{};
		VkSemaphore waitSemaphores[] = { _swapchainSemaphore };
		VkSemaphore signalSemaphores[] = { _presentSemaphores[image_index] };

		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &_presentBuffers[image_index];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		VkResult res = vkQueueSubmit(_graphicsQueue.queue, 1, &submitInfo, _presentFences[image_index]);

		assert(res != VK_ERROR_DEVICE_LOST);

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &_swapchain;
		presentInfo.pImageIndices = &image_index;
		presentInfo.pResults = nullptr;

		vkQueuePresentKHR(_graphicsQueue.queue, &presentInfo);
	}
	/*
	* !@brief Handles recreation of swapchain dependant objects
	*/
	void HandleResize()
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

			//it shouldn't change as I understand, but better to keep it under control
			assert(_swapchainImages.size() == _presentBuffers.size());
		}
	}

private:
	/*
	* !@brief Create VkInstance object to use with glfw context
	* 
	* @return VK_TRUE if creation was successful, VK_FALSE otherwise
	*/
	VkBool32 _initializeInstance()
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
			createInfo.pNext = nullptr;
#endif
		return vkCreateInstance(&createInfo, VK_NULL_HANDLE, &_instance) == VK_SUCCESS;
	}
	/*
	* !@brief Find suitable physical device
	* compatable with required extensions
	* 
	* @return VK_TRUE if suitable device was found, VK_FALSE otherwise
	*/
	VkBool32 _enumeratePhysicalDevices()
	{
		uint32_t devicesCount;
		std::vector<VkPhysicalDevice> devicesArray;

		vkEnumeratePhysicalDevices(_instance, &devicesCount, nullptr);
		devicesArray.resize(devicesCount);
		vkEnumeratePhysicalDevices(_instance, &devicesCount, devicesArray.data());

		const auto& it = std::find_if(devicesArray.cbegin(), devicesArray.cend(), [&, this](const VkPhysicalDevice& itt)
			{
				return this->_enumerateDeviceExtensions(itt, _extensions);
			});

		if (it != devicesArray.end())
		{
			_physicalDevice = *it;
			return VK_TRUE;
		}

		return VK_FALSE;
	}
	/*
	* !@brief Initilizes logical device based on suitable physical device
	* 
	* @return VK_TRUE if device was created successfuly, VK_FALSE otherwise
	*/
	VkBool32 _initializeLogicalDevice()
	{
		assert(_physicalDevice != VK_NULL_HANDLE);

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.sampleRateShading = VK_TRUE;
		deviceFeatures.geometryShader = VK_TRUE;
		deviceFeatures.imageCubeArray = VK_TRUE;
		deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
		deviceFeatures.tessellationShader = VK_TRUE;
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
		deviceFeatures.independentBlend = VK_TRUE;
		deviceFeatures.depthBiasClamp = VK_TRUE;
		deviceFeatures.depthClamp = VK_TRUE;
		deviceFeatures.fullDrawIndexUint32 = VK_TRUE;
		deviceFeatures.shaderFloat64 = VK_TRUE;

		std::vector<VkDeviceQueueCreateInfo> queueInfos;
		std::vector<uint32_t> queueFamilies(std::move(_findQueues({ VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_COMPUTE_BIT })));

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
	/*
	* !@brief Initialize Vulkan Memmory Allocator to handle allocations
	* 
	* @return VK_TRUE if creation is successful, VK_FALSE otherwise
	*/
	VkBool32 _initializeAllocator()
	{
		assert(_logicalDevice != VK_NULL_HANDLE && _instance != VK_NULL_HANDLE && _physicalDevice != VK_NULL_HANDLE);

		VmaAllocatorCreateInfo createInfo{};
		createInfo.device = _logicalDevice;
		createInfo.instance = _instance;
		createInfo.physicalDevice = _physicalDevice;
		createInfo.vulkanApiVersion = VK_API_VERSION_1_2;
		
		return vmaCreateAllocator(&createInfo, &_allocator) == VK_SUCCESS;
	}
	/*
	* !@brief Initializes VkSwapchainKHR object based on the surface capabilities
	* 
	* @return VK_TRUE if initialization was successful, VK_FALSE if window size is zero or initialization failed
	*/
	VkBool32 _initializeSwapchain()
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

		std::vector<uint32_t> queueFamilies(std::move(_findQueues({ VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT })));
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

		return vkCreateSwapchainKHR(_logicalDevice, &createInfo, VK_NULL_HANDLE, &_swapchain) == VK_SUCCESS;
	}
	/*
	* !@brief Retrieve images from specified swapchain and initialize image views for them
	* 
	* @return VK_TRUE if images were retrieved and views created, VK_FALSE if either have failed
	*/
	VkBool32 _initializeSwapchainImages()
	{
		assert(_swapchain != VK_NULL_HANDLE);

		uint32_t imagesCount = 0;
		vkGetSwapchainImagesKHR(_logicalDevice, _swapchain, &imagesCount, nullptr);
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

		for (int i = 0; i < _depthAttachments.size(); i++)
		{
			res = (vmaCreateImage(_allocator, &depthInfo, &allocCreateInfo, &_depthAttachments[i].image, &_depthAttachments[i].memory, VK_NULL_HANDLE) == VK_SUCCESS) & res;
			viewInfo.image = _depthAttachments[i].image;
			res = (vkCreateImageView(_logicalDevice, &viewInfo, VK_NULL_HANDLE, &_depthAttachments[i].view) == VK_SUCCESS) & res;

		}

		return res;
	}
	/*
	* !@brief Initializes simple render pass object with one subpass consisting of one color and one depth attachments
	* 
	* @return VK_TRUE if initialization was successful, VK_FALSE otherwise
	*/
	VkBool32 _initializeRenderPass()
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

		VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depth_ref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

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
	/*
	* !@brief Initialize vector of framebuffers size of swapchain images
	* 
	* @return VK_TRUE if all framebuffers were created successfuly, VK_FALSE otherwise
	*/
	VkBool32 _initializeFramebuffers()
	{
		assert(_swapchainImages.size() > 0 && _renderPass != VK_NULL_HANDLE);

		_framebuffers.resize(_swapchainImages.size());

		VkBool32 res = 1;
		for (size_t i = 0; i < _framebuffers.size(); i++)
		{
			std::array<VkImageView, 2> attachments = { _swapchainViews[i], _depthAttachments[i].view};
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
	/*
	* !@brief Creates command pool for specified queue family
	* 
	* @param[in] targetQueueIndex - index of the queue
	* @param[out] outPool - where to store command pool
	* 
	* @return VK_TRUE if creation was successful, VK_FALSE otherwise
	*/
	VkBool32 _initializeCommandPool(const uint32_t targetQueueIndex,VkCommandPool* outPool)
	{
		VkCommandPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.queueFamilyIndex = targetQueueIndex;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		return vkCreateCommandPool(_logicalDevice, &createInfo, VK_NULL_HANDLE, outPool) == VK_SUCCESS;
	}
	/*
	* !@brief Allocates specified number of command buffers from the pool
	* 
	* @param[in] pool - command pool to allocate from
	* @param[in] count - number of buffers to allocate
	* @param[out] outBuffers - pointer to the VkCommandBuffer object/array
	* 
	* @return VK_TRUE if allocation was successful, VK_FALSE otherwise
	*/
	VkBool32 _allocateBuffers(const VkCommandPool& pool, const uint32_t count, VkCommandBuffer* outBuffers)
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandBufferCount = count;
		allocInfo.commandPool = pool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		return vkAllocateCommandBuffers(_logicalDevice, &allocInfo, outBuffers) == VK_SUCCESS;
	}
	/*
	* !@brief Creates synchronization fence
	* 
	* @param[out] outFence - pointer to store fence at
	* 
	* @return VK_TRUE if creation was successful, VK_FALSE otherwise
	*/
	VkBool32 _createFence(VkFence* outFence)
	{
		VkFenceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		return vkCreateFence(_logicalDevice, &createInfo, VK_NULL_HANDLE, outFence) == VK_SUCCESS;
	}
	/*
	* !@brief Creates synchronization semaphore
	* 
	* @param[out] outSemaphore - pointer to store semaphore at
	* 
	* @return VK_TRUE if creation was successful, VK_FALSE otherwise
	*/
	VkBool32 _createSemaphore(VkSemaphore* outSemaphore)
	{
		VkSemaphoreCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		return vkCreateSemaphore(_logicalDevice, &createInfo, VK_NULL_HANDLE, outSemaphore) == VK_SUCCESS;
	}
	/*
	* !@brief Checks if GPU supports specified extensions
	*
	* @param[in] physicalDevuce - GPU device to test
	* @param[in] desired_extensions - collection of extensions to find
	*
	* @return VK_TRUE if GPU supports specified extensions, VK_FALSE otherwise
	*/
	VkBool32 _enumerateDeviceExtensions(const VkPhysicalDevice& physicalDevice, const std::vector<const char*>& desired_extensions) const
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
	/*
	* !@brief Finds suitable queue indices for specified queue flag bits
	*
	* @param[in] flags - collection of queue flags to find
	*
	* @return collection of family indices ordered respectively to the flags collection
	*/
	std::vector<uint32_t> _findQueues(const std::vector<VkQueueFlagBits>& flags) const
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
	/*
	* !@brief Gets VkQueue object based on it's family index
	*
	* @param[in] familyIndex - family index of desired queue
	*
	* @return structure with VkQueue object and it's respective index packed
	*/
	vkQueueStruct _getVkQueueStruct(const uint32_t& familyIndex) const
	{
		assert(_logicalDevice != VK_NULL_HANDLE);

		vkQueueStruct output{};
		output.index = familyIndex;
		vkGetDeviceQueue(_logicalDevice, familyIndex, 0, &output.queue);

		assert(output.queue != VK_NULL_HANDLE);

		return output;
	}

	std::vector<const char*> _getRequiredExtensions() 
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

	VkDebugUtilsMessengerEXT debugMessenger;
	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	void setupDebugMessenger() {
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(_instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}

	bool checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
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

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}
#endif
};