#include "pch.hpp"
#include "vulkan_api.hpp"

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

VkBool32 CreateFence(const VkDevice& device, VkFence* outFence)
{
	VkFenceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
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

std::vector<uint32_t> FindDeviceQueues(const VkPhysicalDevice& physicalDevice, const std::vector<VkQueueFlagBits>& flags)
{
	uint32_t familiesCount;
	std::vector<uint32_t> output(flags.size());
	std::vector<VkQueueFamilyProperties> queueFamilies;

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familiesCount, VK_NULL_HANDLE);
	queueFamilies.resize(familiesCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familiesCount, queueFamilies.data());

	//reverse to try to find a flag specific queue first and general queue if it doesn't exist
	std::reverse(queueFamilies.begin(), queueFamilies.end());

	int j = 0;
	for (const auto& flag : flags) {
		int i = queueFamilies.size() - 1;
		for (const auto& family : queueFamilies) {
			if ((flag & family.queueFlags) != 0) {
				output[j++] = i;
				break;
			}
			i--;
		}
		assert(i >= 0);
	}

	return output;
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

	VkSwapchainCreateInfoKHR createInfo{};
	VkSurfaceCapabilitiesKHR capabilities{};

	uint32_t temp = 0;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
	VkSurfaceFormatKHR surfaceFormat = GetSurfaceFormat(physicalDevice, surface, desired_format);

	std::vector<uint32_t> queueFamilies(FindDeviceQueues(physicalDevice, { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT }));
	uint32_t desiredImageCount = glm::min(capabilities.minImageCount + 1, capabilities.maxImageCount);

	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.minImageCount = desiredImageCount;
	createInfo.surface = surface;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageArrayLayers = 1;
	createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.clipped = VK_TRUE;
	createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent.width = std::clamp(static_cast<uint32_t>(extent.width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	createInfo.imageExtent.height = std::clamp(static_cast<uint32_t>(extent.height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	createInfo.queueFamilyIndexCount = queueFamilies.size();
	createInfo.pQueueFamilyIndices = queueFamilies.data();

	extent = createInfo.imageExtent;

	if (extent.width == 0 || extent.height == 0)
		return VK_FALSE;

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

VkBool32 CreateFramebuffer(const VkDevice& device, const VkRenderPass& renderPass, const VkExtent2D extents, const std::vector<VkImageView>& attachments, VkFramebuffer* outFramebuffer)
{
	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.attachmentCount = attachments.size();
	createInfo.pAttachments = attachments.data();
	createInfo.layers = 1;
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
	createInfo.vulkanApiVersion = VK_API_VERSION_1_2;

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

VkBool32 CreateLogicalDevice(const VkPhysicalDevice& physicalDevice, const VkPhysicalDeviceFeatures& device_features, const std::vector<const char*>& device_extensions, const std::vector<uint32_t>& queues, VkDevice* outDevice)
{
	assert(physicalDevice != VK_NULL_HANDLE);

	std::vector<VkDeviceQueueCreateInfo> queueInfos;

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : queues) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
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