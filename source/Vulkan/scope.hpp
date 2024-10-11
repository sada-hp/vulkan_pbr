#pragma once
#include "Vulkan/queue.hpp"
#include "Vulkan/vulkan_api.hpp"

enum class ESamplerType
{
	PointClamp,
	PointRepeat,
	PointMirror,
	LinearClamp,
	LinearRepeat,
	LinearMirror,
	BillinearClamp,
	BillinearRepeat,
	BillinearMirror
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

	RenderScope& CreateLowResRenderPass();

	RenderScope& CreateCompositionRenderPass();

	RenderScope& CreatePostProcessRenderPass();

	RenderScope& CreateDescriptorPool(uint32_t setsCount, const std::vector<VkDescriptorPoolSize>& poolSizes);

	void RecreateSwapchain(const VkSurfaceKHR& surface);

	void Destroy();

	VkBool32 IsReadyToUse() const;

	inline const VkDevice& GetDevice() const { return m_LogicalDevice; };

	inline const VmaAllocator& GetAllocator() const { return m_Allocator; };

	inline const VkPhysicalDevice& GetPhysicalDevice() const { return m_PhysicalDevice; };

	inline const VkRenderPass& GetRenderPass() const { return m_RenderPass; };

	inline const VkRenderPass& GetLowResRenderPass() const { return m_RenderPassLR; };
	
	inline const VkRenderPass& GetCompositionPass() const { return m_CompositionPass; };

	inline const VkRenderPass& GetPostProcessPass() const { return m_PostProcessPass; };

	inline const VkSwapchainKHR& GetSwapchain() const { return m_Swapchain; };

	inline const VkExtent2D& GetSwapchainExtent() const { return m_SwapchainExtent; };

	inline const VkDescriptorPool& GetDescriptorPool() const { return m_DescriptorPool; };

	inline const VkFormat GetHDRFormat() const { return VK_FORMAT_R32G32B32A32_SFLOAT; };

	inline const VkFormat GetColorFormat() const { return VK_FORMAT_B8G8R8A8_SRGB; };

	inline const VkFormat GetDepthFormat() const { return VK_FORMAT_D32_SFLOAT; };

	inline const uint32_t& GetMaxFramesInFlight() const { return m_FramesInFlight; };

	const VkSampler& GetSampler(ESamplerType Type) const;

	inline const Queue& GetQueue(VkQueueFlagBits Type) const 
	{
		assert(m_Queues.contains(Type));
		return m_Queues.at(Type);
	}

private:
	std::unordered_map<VkQueueFlagBits, Queue> m_Queues;
	mutable std::unordered_map<ESamplerType, VkSampler> m_Samplers;

	uint32_t m_FramesInFlight = 1u;

	VkDevice m_LogicalDevice = VK_NULL_HANDLE;
	VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
	VmaAllocator m_Allocator = VK_NULL_HANDLE;
	VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
	VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

	VkRenderPass m_RenderPass = VK_NULL_HANDLE;
	VkRenderPass m_RenderPassLR = VK_NULL_HANDLE;
	VkRenderPass m_CompositionPass = VK_NULL_HANDLE;
	VkRenderPass m_PostProcessPass = VK_NULL_HANDLE;

	VkExtent2D m_SwapchainExtent = { 0, 0 };
};