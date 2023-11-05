#pragma once
#include "vulkan_objects/queue.hpp"
#include "vulkan_api.hpp"

enum class EQType
{
	Graphics,
	Transfer,
	Compute
};

class RenderScope
{
public:
	RenderScope() {};
	~RenderScope() { Destroy(); };

	RenderScope& CreatePhysicalDevice(const VkInstance& instance, const std::vector<const char*>& device_extensions);

	RenderScope& CreateLogicalDevice(const VkPhysicalDeviceFeatures& features, const std::vector<const char*>& device_extensions, const std::vector<VkQueueFlagBits>& queues);

	RenderScope& CreateMemoryAllocator(const VkInstance& instance);

	RenderScope& CreateSwapchain(const VkSurfaceKHR& surface);

	RenderScope& CreateDefaultRenderPass();

	RenderScope& RecreateSwapchain(const VkSurfaceKHR& surface);

	void Destroy();

	bool IsReadyToUse() const;

	inline const VkDevice& GetDevice() const { return logicalDevice; };

	inline const VmaAllocator& GetAllocator() const { return allocator; };

	inline const VkPhysicalDevice& GetPhysicalDevice() const { return physicalDevice; };

	inline const VkRenderPass& GetRenderPass() const { return renderPass; };

	inline const VkSwapchainKHR& GetSwapchain() const { return swapchain; };

	inline const VkExtent2D& GetSwapchainExtent() const { return swapchainExtent; };

	inline const VkFormat& GetColorFormat() const { return swapchainFormat; };

	inline const VkFormat& GetDepthFormat() const { return depthFormat; };

	inline const Queue& GetQueue(VkQueueFlagBits Type) const {
		assert(available_queues.contains(Type));
		return available_queues.at(Type);
	}

private:
	std::unordered_map<VkQueueFlagBits, Queue> available_queues;
	VkDevice logicalDevice = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VmaAllocator allocator = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;

	const VkFormat depthFormat = VK_FORMAT_D16_UNORM;
	VkFormat swapchainFormat = VK_FORMAT_R8G8B8A8_SRGB;
	VkExtent2D swapchainExtent = { 0, 0 };
};