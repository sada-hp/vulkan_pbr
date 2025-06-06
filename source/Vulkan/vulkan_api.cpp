#include "pch.hpp"
#include "vulkan_api.hpp"

VkBool32 CreateSyncronizationStruct(const VkDevice& device, const VkCommandPool pool, uint32_t count, uint32_t semcount, VulkanSynchronization* out)
{
	VkBool32 res = 1u;

	for (uint32_t i = 0; i < count; i++)
	{
		res *= ::AllocateCommandBuffers(device, pool, 1, &out[i].Commands);
		res *= ::CreateFence(device, &out[i].Fence, VK_TRUE);

		out[i].Semaphores.resize(semcount);
		for (uint32_t j = 0; j < semcount; j++)
			res *= ::CreateSemaphore(device, &out[i].Semaphores[j]);
	}

	return res;
}

VkBool32 CreateSyncronizationStruct2(const VkDevice& device, const VkCommandPool pool, uint32_t count, uint32_t semcount, VulkanSynchronization* out)
{
	VkBool32 res = 1u;
	for (uint32_t i = 0; i < count; i++)
	{
		res *= ::AllocateCommandBuffers2(device, pool, 1, &out[i].Commands);
		res *= ::CreateFence(device, &out[i].Fence, VK_TRUE);

		out[i].Semaphores.resize(semcount);
		for (uint32_t j = 0; j < semcount; j++)
			res *= ::CreateSemaphore(device, &out[i].Semaphores[j]);
	}

	return res;
}

VkBool32 DestroySyncronizationStruct(const VkDevice& device, const VkCommandPool pool, uint32_t count, VulkanSynchronization* in)
{
	for (uint32_t i = 0; i < count; i++)
	{
		for (uint32_t j = 0; j < in[i].Semaphores.size(); j++)
			vkDestroySemaphore(device, in[i].Semaphores[j], VK_NULL_HANDLE);

		vkDestroyFence(device, in[i].Fence, VK_NULL_HANDLE);
		vkFreeCommandBuffers(device, pool, 1, &in[i].Commands);
	}

	return 1u;
}

VkBool32 CopyBufferToImage(VkCommandBuffer& cmd, const VkImage& image, const VkBuffer& buffer, const VkImageSubresourceRange& subRes, const VkExtent3D& extent, VkImageLayout layout)
{
	VkBufferImageCopy region{};
	region.imageSubresource.aspectMask = subRes.aspectMask;
	region.imageSubresource.baseArrayLayer = subRes.baseArrayLayer;
	region.imageSubresource.layerCount = subRes.layerCount;
	region.imageSubresource.mipLevel = subRes.baseMipLevel;
	region.bufferOffset = 0u;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = extent;
	vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);

	return 1;
}

VkBool32 CreateFence(const VkDevice& device, VkFence* outFence, VkBool32 signaled)
{
	VkFenceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	createInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

	return vkCreateFence(device, &createInfo, VK_NULL_HANDLE, outFence) == VK_SUCCESS;
}

VkBool32 CreateSemaphore(const VkDevice& device, VkSemaphore* outSemaphore)
{
	VkSemaphoreCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	return vkCreateSemaphore(device, &createInfo, VK_NULL_HANDLE, outSemaphore) == VK_SUCCESS;
}

VkBool32 AllocateCommandBuffers(const VkDevice& device, const VkCommandPool& pool, const uint32_t count, VkCommandBuffer* outBuffers)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandBufferCount = count;
	allocInfo.commandPool = pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	return vkAllocateCommandBuffers(device, &allocInfo, outBuffers) == VK_SUCCESS;
}

VkBool32 AllocateCommandBuffers2(const VkDevice& device, const VkCommandPool& pool, const uint32_t count, VkCommandBuffer* outBuffers)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandBufferCount = count;
	allocInfo.commandPool = pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

	return vkAllocateCommandBuffers(device, &allocInfo, outBuffers) == VK_SUCCESS;
}

VkBool32 CreateDescriptorPool(const VkDevice& device, const VkDescriptorPoolSize* poolSizes, const size_t poolSizesCount, const uint32_t setsCount, VkDescriptorPool* outPool)
{
	VkDescriptorPoolCreateInfo dpoolInfo{};
	dpoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpoolInfo.maxSets = setsCount;
	dpoolInfo.poolSizeCount = poolSizesCount;
	dpoolInfo.pPoolSizes = poolSizes;
	dpoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	return vkCreateDescriptorPool(device, &dpoolInfo, VK_NULL_HANDLE, outPool) == VK_SUCCESS;
}

VkBool32 CreateDescriptorLayout(const VkDevice& device, const std::vector<VkDescriptorType> types, VkShaderStageFlags stages, VkDescriptorSetLayout* out)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings(types.size());

	for (uint32_t i = 0; i < types.size(); i++)
	{
		bindings[i].binding = i;
		bindings[i].descriptorCount = 1;
		bindings[i].descriptorType = types[i];
		bindings[i].stageFlags = stages;
	}

	VkDescriptorSetLayoutCreateInfo dsetInfo{};
	dsetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsetInfo.bindingCount = bindings.size();
	dsetInfo.pBindings = bindings.data();
	return vkCreateDescriptorSetLayout(device, &dsetInfo, VK_NULL_HANDLE, out) == VK_SUCCESS;
}

std::vector<uint32_t> FindDeviceQueues(const VkPhysicalDevice& physicalDevice, const std::vector<VkQueueFlagBits>& flags)
{
	uint32_t familiesCount;
	std::vector<uint32_t> output(flags.size(), 0xffffu);
	std::vector<VkQueueFamilyProperties> queueFamilies;

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familiesCount, VK_NULL_HANDLE);
	queueFamilies.resize(familiesCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familiesCount, queueFamilies.data());

	for (int j = 0; j < flags.size(); j++)
	{
		if (flags[j] & VK_QUEUE_COMPUTE_BIT)
		{
			for (int i = 0; i < queueFamilies.size(); i++)
			{
				if ((queueFamilies[i].queueFlags & flags[j]) && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
				{
					output[j] = i;
					break;
				}
			}
		}
		else if (flags[j] & VK_QUEUE_GRAPHICS_BIT)
		{
			for (int i = 0; i < queueFamilies.size(); i++)
			{
				if ((queueFamilies[i].queueFlags & flags[j]) && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)
				{
					output[j] = i;
					break;
				}
			}
		}
		else if (flags[j] & VK_QUEUE_TRANSFER_BIT)
		{
			for (int i = 0; i < queueFamilies.size(); i++)
			{
				if ((queueFamilies[i].queueFlags & flags[j]) && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0 && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
				{
					output[j] = i;
					break;
				}
			}
		}

		if (output[j] == 0xffffu)
		{
			for (int i = 0; i < queueFamilies.size(); i++)
			{
				if (queueFamilies[i].queueFlags & flags[j])
				{
					output[j] = i;
					break;
				}
			}
		}
	}

	return output;
}

uint32_t FindDeviceQueues(const VkPhysicalDevice& physicalDevice, int Bits)
{
	uint32_t familiesCount;
	std::vector<VkQueueFamilyProperties> queueFamilies;

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familiesCount, VK_NULL_HANDLE);
	queueFamilies.resize(familiesCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familiesCount, queueFamilies.data());

	for (int i = 0; i < queueFamilies.size(); i++)
	{
		if ((queueFamilies[i].queueFlags & Bits) != 0)
		{
			return i;
		}
	}

	return -1;
}

VkBool32 EnumerateDeviceExtensions(const VkPhysicalDevice& physicalDevice, const std::vector<const char*>& desired_extensions)
{
	uint32_t extensionCount;

	vkEnumerateDeviceExtensionProperties(physicalDevice, VK_NULL_HANDLE, &extensionCount, VK_NULL_HANDLE);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, VK_NULL_HANDLE, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(desired_extensions.begin(), desired_extensions.end());
	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

VkBool32 CreateSwapchain(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, const VkSurfaceFormatKHR& desired_format, VkExtent2D& extent, VkSwapchainKHR* outSwapchain)
{
	assert(surface != VK_NULL_HANDLE);

	if (extent.width == 0 || extent.height == 0)
		return VK_FALSE;

	VkSwapchainCreateInfoKHR createInfo{};
	VkSurfaceCapabilitiesKHR capabilities{};

	uint32_t temp = 0;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
	VkSurfaceFormatKHR surfaceFormat = GetSurfaceFormat(physicalDevice, surface, desired_format);

	std::vector<uint32_t> queueFamilies = FindDeviceQueues(physicalDevice, { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT });
	std::sort(queueFamilies.begin(), queueFamilies.end());
	queueFamilies.resize(std::distance(queueFamilies.begin(), std::unique(queueFamilies.begin(), queueFamilies.end())));
	uint32_t desiredImageCount = glm::min(capabilities.minImageCount + 1, capabilities.maxImageCount);

	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.minImageCount = desiredImageCount;
	createInfo.surface = surface;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageArrayLayers = 1;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.clipped = VK_TRUE;
#ifndef FORCE_VSYNC
	createInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
#else
	createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
#endif
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent.width = std::clamp(static_cast<uint32_t>(extent.width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	createInfo.imageExtent.height = std::clamp(static_cast<uint32_t>(extent.height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	createInfo.queueFamilyIndexCount = queueFamilies.size();
	createInfo.pQueueFamilyIndices = queueFamilies.data();

	return vkCreateSwapchainKHR(device, &createInfo, VK_NULL_HANDLE, outSwapchain) == VK_SUCCESS;
}

VkSurfaceFormatKHR GetSurfaceFormat(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, const VkSurfaceFormatKHR& desired_format)
{
	VkSurfaceCapabilitiesKHR capabilities{};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;

	uint32_t temp = 0;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &temp, VK_NULL_HANDLE);
	formats.resize(temp);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &temp, formats.data());

	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &temp, VK_NULL_HANDLE);
	presentModes.resize(temp);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &temp, presentModes.data());

	const auto& surfaceFormat = std::find_if(formats.cbegin(), formats.cend(), [&](const VkSurfaceFormatKHR& it) {
		return it.format == desired_format.format && it.colorSpace == desired_format.colorSpace;
	});

	assert(surfaceFormat != formats.cend());

	return *surfaceFormat;
}

VkBool32 CreateCommandPool(const VkDevice& device, const uint32_t targetQueueIndex, VkCommandPool* outPool)
{
	VkCommandPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = targetQueueIndex;
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	return vkCreateCommandPool(device, &createInfo, VK_NULL_HANDLE, outPool) == VK_SUCCESS;
}

VkBool32 CreateFramebuffer(const VkDevice& device, const VkRenderPass& renderPass, const VkExtent3D extents, const std::vector<VkImageView>& attachments, VkFramebuffer* outFramebuffer)
{
	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.attachmentCount = attachments.size();
	createInfo.pAttachments = attachments.data();
	createInfo.layers = extents.depth;
	createInfo.width = extents.width;
	createInfo.height = extents.height;
	createInfo.renderPass = renderPass;

	return vkCreateFramebuffer(device, &createInfo, VK_NULL_HANDLE, outFramebuffer) == VK_SUCCESS;
}

VkBool32 CreateAllocator(const VkInstance& instance, const VkPhysicalDevice& physicalDevice, const VkDevice& device, VmaAllocator* outAllocator)
{
	VmaAllocatorCreateInfo createInfo{};
	createInfo.device = device;
	createInfo.instance = instance;
	createInfo.physicalDevice = physicalDevice;
	createInfo.vulkanApiVersion = VK_API_VERSION_1_3;

	return vmaCreateAllocator(&createInfo, outAllocator) == VK_SUCCESS;
}

VkBool32 EnumeratePhysicalDevices(const VkInstance& instance, const std::vector<const char*>& device_extensions, VkPhysicalDevice* outPhysicalDevice)
{
	uint32_t devicesCount;
	std::vector<VkPhysicalDevice> devicesArray;

	vkEnumeratePhysicalDevices(instance, &devicesCount, VK_NULL_HANDLE);
	devicesArray.resize(devicesCount);
	vkEnumeratePhysicalDevices(instance, &devicesCount, devicesArray.data());

	const auto& it = std::find_if(devicesArray.cbegin(), devicesArray.cend(), [&](const VkPhysicalDevice& itt) {
		return EnumerateDeviceExtensions(itt, device_extensions);
	});

	if (it != devicesArray.end()) {
		*outPhysicalDevice = *it;
		return VK_TRUE;
	}

	return VK_FALSE;
}

VkBool32 CreateLogicalDevice(const VkPhysicalDevice& physicalDevice, const VkPhysicalDeviceFeatures& device_features, const std::vector<const char*>& device_extensions, const std::vector<uint32_t>& queues, VkDevice* outDevice, void* pNext)
{
	assert(physicalDevice != VK_NULL_HANDLE);

	std::vector<VkDeviceQueueCreateInfo> queueInfos;

	float queuePriority = 1.0f;
	for (uint32_t i = 0; i < queues.size(); i++) {

		if (i > 0 && std::find(queues.cbegin(), std::next(queues.cbegin(), i), queues[i]) != std::next(queues.cbegin(), i))
		{
			continue;
		}

		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queues[i];
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pEnabledFeatures = &device_features;
	createInfo.ppEnabledExtensionNames = device_extensions.data();
	createInfo.enabledExtensionCount = device_extensions.size();
	createInfo.pQueueCreateInfos = queueInfos.data();
	createInfo.queueCreateInfoCount = queueInfos.size();
	createInfo.pNext = pNext;

	return vkCreateDevice(physicalDevice, &createInfo, VK_NULL_HANDLE, outDevice) == VK_SUCCESS;
}

VkBool32 CreateRenderPass(const VkDevice& device, const VkRenderPassCreateInfo& info, VkRenderPass* outRenderPass)
{
	return vkCreateRenderPass(device, &info, VK_NULL_HANDLE, outRenderPass) == VK_SUCCESS;
}

VkBool32 BeginOneTimeSubmitCmd(VkCommandBuffer& cmd)
{
	VkCommandBufferBeginInfo cmdBegin{};
	cmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	return vkBeginCommandBuffer(cmd, &cmdBegin) == VK_SUCCESS;
}

VkBool32 EndCommandBuffer(VkCommandBuffer& cmd)
{
	return vkEndCommandBuffer(cmd) == VK_SUCCESS;
}

std::vector<unsigned char> AnyTypeToBytes(std::any target)
{
	std::vector<unsigned char> buf;

	if (target.type().name() == typeid(int).name()) {
		int value = std::any_cast<int>(target);
		buf.resize(sizeof(int));

		memcpy(buf.data(), &value, sizeof(int));
	}
	else if (target.type().name() == typeid(float).name()) {
		float value = std::any_cast<float>(target);
		buf.resize(sizeof(float));

		memcpy(buf.data(), &value, sizeof(float));
	}
	else if (target.type().name() == typeid(uint32_t).name()) {
		uint32_t value = std::any_cast<uint32_t>(target);
		buf.resize(sizeof(uint32_t));

		memcpy(buf.data(), &value, sizeof(uint32_t));
	}
	else if (target.type().name() == typeid(double).name()) {
		double value = std::any_cast<double>(target);
		buf.resize(sizeof(double));

		memcpy(buf.data(), &value, sizeof(double));
	}

	return buf;
}