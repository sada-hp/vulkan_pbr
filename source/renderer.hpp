#pragma once
#include "core.hpp"
#include "entt/entt.hpp"
#include "shader_objects/general_object.hpp"
#include "shader_objects/pbr_object.hpp"
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
#include "shapes.hpp"

#if DEBUG == 1
#define VALIDATION
#endif

namespace GR
{
	struct Camera
	{
		GRComponents::Projection Projection;
		GRComponents::Transform View;

	private:
		friend class VulkanBase;

		TMat4 get_view_matrix() const
		{
			return glm::lookAt(View.GetOffset(), View.GetOffset() + View.GetForward(), View.GetUp());
		}

		TMat4 get_projection_matrix() const
		{
			return Projection.matrix;
		}

		TMat4 get_view_projection() const
		{
			return Projection.matrix * View.matrix;
		}
	};
};

class VulkanBase
{
	TVector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	TVector<VkImage> swapchainImages = {};
	TVector<VkImageView> swapchainViews = {};
	TVector<TAuto<VulkanImage>> depthAttachments = {};
	TVector<TAuto<VulkanImage>> hdrAttachments = {};
	TVector<VkFramebuffer> framebuffers = {};
	TVector<VkFence> presentFences = {};
	TVector<VkSemaphore> presentSemaphores = {};
	TVector<VkSemaphore> swapchainSemaphores = {};
	TVector<VkCommandBuffer> presentBuffers = {};
	TVector<TAuto<Pipeline>> HDRPipelines;
	TVector<TAuto<DescriptorSet>> HDRDescriptors;

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	RenderScope Scope;
	GLFWwindow* glfwWindow = VK_NULL_HANDLE;

	TAuto<Buffer> ubo = {};		
	TAuto<Buffer> view = {};
	TAuto<Buffer> cloud_layer = {};

	TAuto<GraphicsObject> volume;
	TAuto<GraphicsObject> skybox;

	TAuto<VulkanImage> CloudShape;
	TAuto<VulkanImage> CloudDetail;

	TAuto<VulkanImage> ScatteringLUT;
	TAuto<VulkanImage> IrradianceLUT;
	TAuto<VulkanImage> Transmittance;

	uint32_t swapchain_index = 0;

#ifdef INCLUDE_GUI
	VkDescriptorPool imguiPool;
#endif

public:
	CloudProfileLayer CloudLayer;

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

	entt::entity AddMesh(const std::string& mesh_path);

	entt::entity AddShape(const GRShape::Shape& descriptor);

	TAuto<VulkanImage> LoadImage(const std::string& path, VkFormat format);

	void RegisterEntityUpdate(entt::entity ent);
	/*
	* !@brief Should be modified to control the scene
	*/
	GR::Camera camera;
	TVec3 SunDirection = glm::normalize(TVec3(1.0));

private:
	entt::registry& registry;

	TShared<VulkanImage> defaultAlbedo = VK_NULL_HANDLE;
	TShared<VulkanImage> defaultNormal = VK_NULL_HANDLE;
	TShared<VulkanImage> defaultRoughness = VK_NULL_HANDLE;
	TShared<VulkanImage> defaultMetallic = VK_NULL_HANDLE;
	TShared<VulkanImage> defaultAO = VK_NULL_HANDLE;

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

	VkBool32 create_hdr_pipeline();
	/*
	* !@brief Handles creation of an object and a skybox
	*/
	VkBool32 prepare_scene();

	void render_objects(VkCommandBuffer cmd);

	VkBool32 setup_precompute();

	TVector<const char*> getRequiredExtensions();

	TAuto<DescriptorSet> create_pbr_set(const RenderScope& Scope, const VulkanImage& albedo, const VulkanImage& normal, const VulkanImage& roughness, const VulkanImage& metallic, const VulkanImage& ao);

	TAuto<Pipeline> create_pbr_pipeline(const RenderScope& Scope, const DescriptorSet& set);


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