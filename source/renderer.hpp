#pragma once
#include "core.hpp"
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
#include "structs.hpp"

#if DEBUG == 1
	#define VALIDATION
#endif

namespace GR
{
	struct Camera
	{
		GRComponents::Projection Projection;
		GRComponents::Transform View;

		GRAPI TVec3 GetWorldPosition()
		{
			return glm::transpose(View.GetRotation()) * View.GetOffset();
		}

		GRAPI void SetWorldPosition(TVec3 Offset)
		{
			View.SetOffset(View.GetRotation() * Offset);
		}

	private:
		friend class VulkanBase;

		TMat4 get_view_projection() const
		{
			return Projection.matrix * View.matrix;
		}
	};
};

struct VulkanGraphicsObject
{
	VulkanGraphicsObject(RenderScope& Scope)
		: descriptor_set(Scope), pipeline(Scope)
	{
	}

	TAuto<Mesh> mesh;
	TAuto<Image> texture;

	DescriptorSet descriptor_set;
	GraphicsPipeline pipeline;
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
	TVector<VkSemaphore> swapchainSemaphores = {};
	TVector<VkCommandBuffer> presentBuffers = {};

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	RenderScope Scope;
	GLFWwindow* glfwWindow = VK_NULL_HANDLE;

	TAuto<Buffer> ubo = {};		
	TAuto<Buffer> view = {};
	TAuto<Buffer> cloud_layer = {};

	TAuto<GraphicsObject> volume;
	TAuto<GraphicsObject> skybox;
	TAuto<Image> CloudShape;
	TAuto<Image> CloudDetail;
	TAuto<Image> Gradient;

	TAuto<ComputePipeline> TransDeltaLUT;

	uint32_t swapchain_index = 0;

	VkDescriptorPool imguiPool;

public:
	SCloudLayer CloudLayer;

	VulkanBase(GLFWwindow* window, entt::registry& registry);

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
	/*
	* !@brief Should be modified to control the scene
	*/
	GR::Camera camera;
	TVec3 SunDirection = glm::normalize(TVec3(1.0));

private:
	entt::registry& registry;
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

	VkBool32 setup_precompute();

	void run_precompute(VkCommandBuffer cmd);

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