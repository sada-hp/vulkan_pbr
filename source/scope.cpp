#include "pch.hpp"
#include "scope.hpp"

RenderScope& RenderScope::CreatePhysicalDevice(const VkInstance& instance, const std::vector<const char*>& device_extensions)
{
	assert(physicalDevice == VK_NULL_HANDLE);

	::EnumeratePhysicalDevices(instance, device_extensions, &physicalDevice);
	return *this;
}

RenderScope& RenderScope::CreateLogicalDevice(const VkPhysicalDeviceFeatures& features, const std::vector<const char*>& device_extensions, const std::vector<VkQueueFlagBits>& queues)
{
	assert(physicalDevice != VK_NULL_HANDLE && logicalDevice == VK_NULL_HANDLE);

	::CreateLogicalDevice(physicalDevice, features, device_extensions, FindDeviceQueues(physicalDevice, queues), &logicalDevice);

	for (const auto& queue : queues) {
		uint32_t queueFamilies = FindDeviceQueues(physicalDevice, { queue })[0];
		available_queues.emplace(std::piecewise_construct, std::forward_as_tuple(queue), std::forward_as_tuple(logicalDevice, queueFamilies));
	}
	return *this;
}

RenderScope& RenderScope::CreateMemoryAllocator(const VkInstance& instance)
{
	assert(physicalDevice != VK_NULL_HANDLE && logicalDevice != VK_NULL_HANDLE && allocator == VK_NULL_HANDLE);

	::CreateAllocator(instance, physicalDevice, logicalDevice, &allocator);

	return *this;
}

RenderScope& RenderScope::CreateSwapchain(const VkSurfaceKHR& surface)
{
	assert(physicalDevice != VK_NULL_HANDLE && logicalDevice != VK_NULL_HANDLE && swapchain == VK_NULL_HANDLE);

	VkSurfaceCapabilitiesKHR surfaceCapabilities{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

	if (::CreateSwapchain(logicalDevice, physicalDevice, surface, { swapchainFormat , VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }, surfaceCapabilities.currentExtent, &swapchain)) {
		vkGetSwapchainImagesKHR(logicalDevice, swapchain, &framesInFlight, VK_NULL_HANDLE);
	}
	swapchainExtent = surfaceCapabilities.currentExtent;

	return *this;
}

RenderScope& RenderScope::CreateDefaultRenderPass()
{
	VkRenderPassCreateInfo createInfo{};
	std::array<VkAttachmentDescription, 2> attachments;

	//Color attachment
	attachments[0].format = swapchainFormat;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].flags = 0;
	// Depth attachment
	attachments[1].format = depthFormat;
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

	::CreateRenderPass(logicalDevice, createInfo, &renderPass);

	return *this;
}

RenderScope& RenderScope::CreateDescriptorPool(uint32_t setsCount, const std::vector<VkDescriptorPoolSize>& poolSizes)
{
	if (descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(logicalDevice, descriptorPool, VK_NULL_HANDLE);
	}

	::CreateDescriptorPool(logicalDevice, poolSizes.data(), poolSizes.size(), setsCount, &descriptorPool);

	return *this;
}

void RenderScope::RecreateSwapchain(const VkSurfaceKHR& surface)
{
	vkDestroySwapchainKHR(logicalDevice, swapchain, VK_NULL_HANDLE);
	swapchain = VK_NULL_HANDLE;

	CreateSwapchain(surface);
}

void RenderScope::Destroy()
{
	available_queues.clear();

	if (descriptorPool != VK_NULL_HANDLE)
		vkDestroyDescriptorPool(logicalDevice, descriptorPool, VK_NULL_HANDLE);
	if (renderPass != VK_NULL_HANDLE)
		vkDestroyRenderPass(logicalDevice, renderPass, VK_NULL_HANDLE);
	if (swapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(logicalDevice, swapchain, VK_NULL_HANDLE);
	if (allocator != VK_NULL_HANDLE)
		vmaDestroyAllocator(allocator);
	if (logicalDevice != VK_NULL_HANDLE)
		vkDestroyDevice(logicalDevice, VK_NULL_HANDLE);

	descriptorPool = VK_NULL_HANDLE;
	renderPass = VK_NULL_HANDLE;
	swapchain = VK_NULL_HANDLE;
	allocator = VK_NULL_HANDLE;
	logicalDevice = VK_NULL_HANDLE;
}

VkBool32 RenderScope::IsReadyToUse() const
{
	return logicalDevice != VK_NULL_HANDLE
		&& physicalDevice != VK_NULL_HANDLE
		&& allocator != VK_NULL_HANDLE
		&& swapchain != VK_NULL_HANDLE
		&& renderPass != VK_NULL_HANDLE
		&& descriptorPool != VK_NULL_HANDLE;
}