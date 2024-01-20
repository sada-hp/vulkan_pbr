#pragma once
#include "object.hpp"
#include "scope.hpp"
#include "vulkan_objects/queue.hpp"
#include "vulkan_objects/buffer.hpp"
#include "vulkan_objects/image.hpp"
#include "vulkan_objects/mesh.hpp"
#include "file_manager.hpp"
#include "vulkan_api.hpp"
#include "noise.hpp"

#if DEBUG == 1
	#define VALIDATION
#endif

struct ViewCameraStruct
{
	const TMat4& GetViewProjection() const
	{
		//cache
		static TVec3 old_pos = glm::vec3(0.f);
		static TQuat old_ori = glm::quat_cast(glm::mat4(1.f));

		if (position != old_pos || orientation != old_ori)
		{
			VP = projection * glm::translate(glm::mat4_cast(orientation), position);

			old_pos = position;
			old_ori = orientation;
		}

		return VP;
	}

	TVec3 position = TVec3(0.f);
	TQuat orientation = glm::quat_cast(glm::mat4(1.f));

private:
	mutable TMat4 VP = glm::mat4(1.f);
	TMat4 projection = glm::mat4(1.f);

	void set_projection(const TMat4& proj = TMat4(1.f))
	{
		projection = proj;
		VP = projection * glm::translate(glm::mat4_cast(orientation), position);
	}

	friend class VulkanBase;
};

class VulkanBase
{
	TVector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	TVector<VkImage> swapchainImages = {};
	TVector<VkImageView> swapchainViews = {};
	TVector<TAuto<Image>> depthAttachments = {};
	TVector<VkFramebuffer> framebuffers = {};
	TVector<VkFence> presentFences = {};
	TVector<VkSemaphore> presentSemaphores = {};
	TVector<VkCommandBuffer> presentBuffers = {};
	VkSemaphore swapchainSemaphore = VK_NULL_HANDLE;

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	RenderScope Scope;
	GLFWwindow* glfwWindow = VK_NULL_HANDLE;

	TAuto<Buffer> ubo = {};

	TAuto<GraphicsObject> volume;
	TAuto<GraphicsObject> skybox;
	TAuto<Image> CloudShape;

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
	void Step();
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

	TVector<const char*> getRequiredExtensions();

#ifdef VALIDATION
	VkDebugUtilsMessengerEXT debugMessenger;
	const TVector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

	VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

	void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	void setupDebugMessenger();

	VkBool32 checkValidationLayerSupport() const;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
#endif
};