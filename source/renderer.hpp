#pragma once
#include "entt/entt.hpp"
#include "object.hpp"
#include "scope.hpp"
#include "vulkan_objects/queue.hpp"
#include "vulkan_objects/buffer.hpp"
#include "vulkan_objects/image.hpp"
#include "vulkan_objects/mesh.hpp"
#include "file_manager.hpp"
#include "vulkan_api.hpp"
#include "noise.hpp"
#include "components.hpp"

#if DEBUG == 1
	#define VALIDATION
#endif

struct ViewCameraStruct
{
	const TMat4& GetViewProjection() const
	{
		return VP;
	}

	const TMat4& GetProjectionMatrix() const
	{
		return projection;
	}

	const TMat4& GetViewMatrix() const
	{
		return view;
	}

	const TVec3& Translate(const TVec3& offset)
	{
		position += offset * orientation;
		recalculate();

		return position;
	}

	const TVec3& SetPosition(const TVec3& new_position)
	{
		position = new_position;
		recalculate();

		return position;
	}

	const TVec3& GetPosition()
	{
		return position;
	}

	const TQuat& Rotate(const TQuat& angle)
	{
		orientation = angle * orientation;
		pitch_yaw_roll = glm::eulerAngles(orientation);
		recalculate();

		return orientation;
	}

	const TQuat& Rotate(const TVec3& pyr)
	{
		pitch_yaw_roll += pyr;

		orientation = glm::quat_cast(glm::mat3(1.f));
		orientation = orientation * glm::angleAxis(glm::radians(pitch_yaw_roll.x), glm::vec3(1, 0, 0));
		orientation = orientation * glm::angleAxis(glm::radians(pitch_yaw_roll.y), glm::vec3(0, 1, 0));
		orientation = orientation * glm::angleAxis(glm::radians(pitch_yaw_roll.z), glm::vec3(0, 0, 1));
		recalculate();

		return orientation;
	}

	const TQuat& SetRotation(const TQuat& angle)
	{
		orientation = angle;
		pitch_yaw_roll = glm::eulerAngles(orientation);
		recalculate();

		return orientation;
	}

	const TQuat& SetRotation(const TVec3& pyr)
	{
		pitch_yaw_roll = pyr;

		orientation = glm::quat_cast(glm::mat3(1.f));
		orientation = orientation * glm::angleAxis(glm::radians(pitch_yaw_roll.x), glm::vec3(1, 0, 0));
		orientation = orientation * glm::angleAxis(glm::radians(pitch_yaw_roll.y), glm::vec3(0, 1, 0));
		orientation = orientation * glm::angleAxis(glm::radians(pitch_yaw_roll.z), glm::vec3(0, 0, 1));
		recalculate();

		return orientation;
	}

	const TQuat& GetOrientation()
	{
		return orientation;
	}

private:
	TVec3 position = TVec3(0.f);
	TVec3 pitch_yaw_roll = TVec3(0.f);
	TQuat orientation = glm::quat_cast(glm::mat3(1.f));
	mutable TMat4 projection = glm::mat4(1.f);
	mutable TMat4 view = glm::mat4(1.f);
	mutable TMat4 VP = glm::mat4(1.f);

	void set_projection(const TMat4& proj = TMat4(1.f))
	{
		projection = proj;
		recalculate();
	}

	void recalculate()
	{
		view = glm::translate(glm::mat4_cast(orientation), position);
		VP = projection * view;
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
	TAuto<Buffer> view = {};

	TAuto<GraphicsObject> volume;
	TAuto<GraphicsObject> skybox;
	//TAuto<GraphicsObject> sword;
	TAuto<Image> CloudShape;
	TAuto<Image> CloudDetail;
	TAuto<Image> WeatherImage;
	TAuto<Image> Gradient;

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
	void Step(float DeltaTime);
	/*
	* !@brief Handles recreation of swapchain dependant objects
	*/
	void HandleResize();

	entt::entity AddGraphicsObject(const std::string& mesh_path, const std::string& texture_path);

	template<typename T, typename... Args>
	T& EmplaceComponent(entt::entity entity, Args&& ...args);

	template<typename... Type>
	decltype(auto) GetComponent(const entt::entity entt)
	{
		return registry.get<Type...>(entt);
	}

private:
	entt::registry registry;
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

	void render_objects(VkCommandBuffer cmd);

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