#pragma once
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <vma/vk_mem_alloc.h>
#include <vector>
#include <array>
#include <string>

#if DEBUG == 1
	#define _VkValidation
#endif

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

class VulkanBase
{
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

	VkPipeline _pipeline = VK_NULL_HANDLE;
	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

public:

	VulkanBase(GLFWwindow* window);

	~VulkanBase() noexcept;
	/*
	* !@brief Renders the next frame of simulation
	*/
	void Step() const;
	/*
	* !@brief Handles recreation of swapchain dependant objects
	*/
	void HandleResize();

private:
	/*
	* !@brief Create VkInstance object to use with glfw context
	* 
	* @return VK_TRUE if creation was successful, VK_FALSE otherwise
	*/
	VkBool32 _initializeInstance();
	/*
	* !@brief Find suitable physical device
	* compatable with required extensions
	* 
	* @return VK_TRUE if suitable device was found, VK_FALSE otherwise
	*/
	VkBool32 _enumeratePhysicalDevices();
	/*
	* !@brief Initilizes logical device based on suitable physical device
	* 
	* @return VK_TRUE if device was created successfuly, VK_FALSE otherwise
	*/
	VkBool32 _initializeLogicalDevice();
	/*
	* !@brief Initialize Vulkan Memmory Allocator to handle allocations
	* 
	* @return VK_TRUE if creation is successful, VK_FALSE otherwise
	*/
	VkBool32 _initializeAllocator();
	/*
	* !@brief Initializes VkSwapchainKHR object based on the surface capabilities
	* 
	* @return VK_TRUE if initialization was successful, VK_FALSE if window size is zero or initialization failed
	*/
	VkBool32 _initializeSwapchain();
	/*
	* !@brief Retrieve images from specified swapchain and initialize image views for them
	* 
	* @return VK_TRUE if images were retrieved and views created, VK_FALSE if either have failed
	*/
	VkBool32 _initializeSwapchainImages();
	/*
	* !@brief Initializes simple render pass object with one subpass consisting of one color and one depth attachments
	* 
	* @return VK_TRUE if initialization was successful, VK_FALSE otherwise
	*/
	VkBool32 _initializeRenderPass();
	/*
	* !@brief Initialize vector of framebuffers size of swapchain images
	* 
	* @return VK_TRUE if all framebuffers were created successfuly, VK_FALSE otherwise
	*/
	VkBool32 _initializeFramebuffers();
	/*
	* !@brief Creates command pool for specified queue family
	* 
	* @param[in] targetQueueIndex - index of the queue
	* @param[out] outPool - where to store command pool
	* 
	* @return VK_TRUE if creation was successful, VK_FALSE otherwise
	*/
	VkBool32 _initializeCommandPool(const uint32_t targetQueueIndex, VkCommandPool* outPool);
	/*
	* !@brief Allocates specified number of command buffers from the pool
	* 
	* @param[in] pool - command pool to allocate from
	* @param[in] count - number of buffers to allocate
	* @param[out] outBuffers - pointer to the VkCommandBuffer object/array
	* 
	* @return VK_TRUE if allocation was successful, VK_FALSE otherwise
	*/
	VkBool32 _allocateBuffers(const VkCommandPool& pool, const uint32_t count, VkCommandBuffer* outBuffers);
	/*
	* !@brief Creates synchronization fence
	* 
	* @param[out] outFence - pointer to store fence at
	* 
	* @return VK_TRUE if creation was successful, VK_FALSE otherwise
	*/
	VkBool32 _createFence(VkFence* outFence);
	/*
	* !@brief Creates synchronization semaphore
	* 
	* @param[out] outSemaphore - pointer to store semaphore at
	* 
	* @return VK_TRUE if creation was successful, VK_FALSE otherwise
	*/
	VkBool32 _createSemaphore(VkSemaphore* outSemaphore);
	/*
	* !@brief Checks if GPU supports specified extensions
	*
	* @param[in] physicalDevuce - GPU device to test
	* @param[in] desired_extensions - collection of extensions to find
	*
	* @return VK_TRUE if GPU supports specified extensions, VK_FALSE otherwise
	*/
	VkBool32 _enumerateDeviceExtensions(const VkPhysicalDevice& physicalDevice, const std::vector<const char*>& desired_extensions) const;
	/*
	* !@brief Finds suitable queue indices for specified queue flag bits
	*
	* @param[in] flags - collection of queue flags to find
	*
	* @return collection of family indices ordered respectively to the flags collection
	*/
	std::vector<uint32_t> _findQueues(const std::vector<VkQueueFlagBits>& flags) const;
	/*
	* !@brief Gets VkQueue object based on it's family index
	*
	* @param[in] familyIndex - family index of desired queue
	*
	* @return structure with VkQueue object and it's respective index packed
	*/
	vkQueueStruct _getVkQueueStruct(const uint32_t& familyIndex) const;

	std::vector<const char*> _getRequiredExtensions();

#ifdef _VkValidation

	VkDebugUtilsMessengerEXT debugMessenger;
	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	void setupDebugMessenger();

	VkBool32 checkValidationLayerSupport() const;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
#endif
};