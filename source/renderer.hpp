#pragma once
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vma/vk_mem_alloc.h>
#include <vector>
#include <array>
#include <string>

#if DEBUG == 1
	#define _VkValidation
#endif

struct ViewCameraStruct
{
	const glm::mat4& get_ubo(const glm::mat4& proj = glm::mat4(1.f)) const
	{
		//cache
		static glm::vec3 old_pos = glm::vec3(0.f);
		static glm::vec3 old_ori = glm::vec3(0.f);

		if (position != old_pos || orientation != old_ori)
		{
			glm::quat q = glm::quat_cast(glm::mat3(1.f));
			q = q * glm::angleAxis(glm::radians(orientation.x), glm::vec3(1, 0, 0));
			q = q * glm::angleAxis(glm::radians(orientation.y), glm::vec3(0, 1, 0));
			q = q * glm::angleAxis(glm::radians(orientation.z), glm::vec3(0, 0, 1));

			ubo = proj * glm::translate(glm::mat4_cast(q), position);

			old_pos = position;
			old_ori = orientation;
		}

		return ubo;
	}
	//!@brief XYZ coordinates
	glm::vec3 position = glm::vec3(0.f);
	//!@brief Euler angles PYR
	glm::vec3 orientation = glm::vec3(0.f);
private:
	mutable glm::mat4 ubo = glm::mat4(1.f);
};

struct vkQueueStruct
{
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t index = 0;
};

struct vkImageStruct
{
	void destroy(const VkDevice& device, const VmaAllocator& allocator)
	{
		vkDestroyImageView(device, view, VK_NULL_HANDLE);
		vmaDestroyImage(allocator, image, memory);
	}

	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VmaAllocation memory = VK_NULL_HANDLE;
};

struct vkBufferStruct
{
	void create(const VmaAllocator& allocator, const VkBufferCreateInfo* createInfo, const VmaAllocationCreateInfo* allocCreateInfo)
	{
		vmaCreateBuffer(allocator, createInfo, allocCreateInfo, &buffer, &memory, &allocInfo);
		descriptorInfo.buffer = buffer;
		descriptorInfo.offset = 0;
		descriptorInfo.range = createInfo->size;
	}

	void destroy(const VmaAllocator& allocator)
	{
		vmaDestroyBuffer(allocator, buffer, memory);
		allocInfo = {};
		descriptorInfo = {};
	}

	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation memory = VK_NULL_HANDLE;
	VmaAllocationInfo allocInfo = {};
	VkDescriptorBufferInfo descriptorInfo = {};
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

	vkBufferStruct _ubo = {};

	VkDescriptorSet _descriptorSet = VK_NULL_HANDLE;
	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout _descriptorLayout = VK_NULL_HANDLE;
public:
	/*
	* !@brief Should be modified to control the scene
	*/
	ViewCameraStruct camera;

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