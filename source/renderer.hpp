#pragma once
#include "object.hpp"
#include "scope.hpp"
#include "vulkan_objects/queue.hpp"
#include "vulkan_objects/buffer.hpp"
#include "vulkan_objects/image.hpp"
#include "vulkan_objects/mesh.hpp"
#include "file_manager.hpp"
#include "vulkan_api.hpp"

#if DEBUG == 1
	#define VALIDATION
#endif

using ObjectPosition = glm::vec3;
using ObjectOrientation = glm::quat;
using TransformMatrix = glm::mat4;

struct ViewCameraStruct
{
	const TransformMatrix& GetViewProjection() const
	{
		//cache
		static ObjectPosition old_pos = glm::vec3(0.f);
		static ObjectOrientation old_ori = glm::quat_cast(glm::mat4(1.f));

		if (position != old_pos || orientation != old_ori)
		{
			VP = projection * glm::translate(glm::mat4_cast(orientation), position);

			old_pos = position;
			old_ori = orientation;
		}

		return VP;
	}

	ObjectPosition position = glm::vec3(0.f);
	ObjectOrientation orientation = glm::quat_cast(glm::mat4(1.f));

private:
	mutable TransformMatrix VP = glm::mat4(1.f);
	glm::mat4 projection = glm::mat4(1.f);

	void set_projection(const glm::mat4& proj = glm::mat4(1.f))
	{
		projection = proj;
		VP = projection * glm::translate(glm::mat4_cast(orientation), position);
	}

	friend class VulkanBase;
};

class VulkanBase
{
	std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	std::vector<VkImage> swapchainImages = {};
	std::vector<VkImageView> swapchainViews = {};
	std::vector<std::unique_ptr<Image>> depthAttachments = {};
	std::vector<VkFramebuffer> framebuffers = {};
	std::vector<VkFence> presentFences = {};
	std::vector<VkSemaphore> presentSemaphores = {};
	std::vector<VkCommandBuffer> presentBuffers = {};
	VkSemaphore swapchainSemaphore = VK_NULL_HANDLE;

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	RenderScope Scope;
	GLFWwindow* glfwWindow = VK_NULL_HANDLE;

	std::unique_ptr<Buffer> ubo = {};

	std::unique_ptr<GraphicsObject> sword;
	std::unique_ptr<GraphicsObject> skybox;
public:
	/*
	* !@brief Should be modified to control the scene
	*/
	ViewCameraStruct camera;
	/*
	* !@brief User defined pointer, renderer itself does not reference it
	*/
	void* userPointer1 = nullptr;
	/*
	* !@brief User defined pointer, renderer itself does not reference it
	*/
	void* userPointer2 = nullptr;
	/*
	* !@brief User defined pointer, renderer itself does not reference it
	*/
	void* userPointer3 = nullptr;
	/*
	* !@brief User defined pointer, renderer itself does not reference it
	*/
	void* userPointer4 = nullptr;

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
	std::unique_ptr<FileManager> fileManager;
	/*
	* !@brief Create VkInstance object to use with glfw context
	* 
	* @return VK_TRUE if creation was successful, VK_FALSE otherwise
	*/
	VkBool32 create_instance();
	/*
	* !@brief Retrieve images from specified swapchain and initialize image views for them
	* 
	* @return VK_TRUE if images were retrieved and views created, VK_FALSE if either have failed
	*/
	VkBool32 create_swapchain_images();
	/*
	* !@brief Initialize vector of framebuffers size of swapchain images
	* 
	* @return VK_TRUE if all framebuffers were created successfuly, VK_FALSE otherwise
	*/
	VkBool32 create_framebuffers();
	/*
	* !@brief Handles creation of an object and a skybox
	*/
	VkBool32 prepare_scene();

	std::vector<const char*> getRequiredExtensions();

#ifdef VALIDATION
	VkDebugUtilsMessengerEXT debugMessenger;
	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

	VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

	void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	void setupDebugMessenger();

	VkBool32 checkValidationLayerSupport() const;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
#endif
};