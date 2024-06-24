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
	// !@brief Defined in renderer.cpp
	VulkanBase(GLFWwindow* window, entt::registry& registry);

	// !@brief Defined in renderer.cpp
	~VulkanBase() noexcept;
	/*
	* !@brief Renders the next frame of simulation. Defined in renderer.cpp.
	*/
	void Step(float DeltaTime);
	/*
	* !@brief Handles recreation of swapchain dependant objects. Defined in renderer.cpp.
	*/
	void HandleResize();
	/*
	* !@brief Load mesh model to the scene. Defined in renderer.cpp.
	* 
	* @param[in] mesh_path - local path to the mesh file (for supported formats look at assimp)
	* 
	* @return New entity handle
	*/
	entt::entity AddMesh(const std::string& mesh_path);
	/*
	* !@brief Generate and add a simple shape to the scene. Defined in renderer.cpp.
	* 
	* @param[in] descriptor - description of the shape (aka Sphere, Plane, Cube)
	* 
	* @return New entity handle
	*/
	entt::entity AddShape(const GRShape::Shape& descriptor);

	TAuto<VulkanImage> LoadImage(const std::string& path, VkFormat format);

	void Wait();
	/*
	* !@brief Should be modified to control the scene
	*/
	GR::Camera camera;
	TVec3 SunDirection = glm::normalize(TVec3(1.0));

private:
	entt::registry& registry;

	TShared<VulkanImage> defaultWhite = VK_NULL_HANDLE;
	TShared<VulkanImage> defaultBlack = VK_NULL_HANDLE;
	TShared<VulkanImage> defaultNormal = VK_NULL_HANDLE;

	// !@brief Defined in initialization.cpp
	VkBool32 create_instance();

	// !@brief Defined in initialization.cpp
	VkBool32 create_swapchain_images();

	// !@brief Defined in initialization.cpp
	VkBool32 create_framebuffers();

	// !@brief Defined in initialization.cpp
	VkBool32 create_hdr_pipeline();

	// !@brief Defined in initialization.cpp
	VkBool32 prepare_renderer_resources();

	// !@brief Defined in initialization.cpp
	TVector<const char*> getRequiredExtensions();

	// !@brief Defined in precompute.cpp
	VkBool32 atmosphere_precompute();

	// !@brief Defined in precompute.cpp
	VkBool32 volumetric_precompute();

	// !@brief Defined in pbr_controls.cpp
	void update_pipeline(entt::entity ent);

	// !@brief Defined in pbr_controls.cpp
	TAuto<DescriptorSet> create_pbr_set(const VulkanImage& albedo, const VulkanImage& normal, const VulkanImage& roughness, const VulkanImage& metallic, const VulkanImage& ao, const VulkanImage& displacement);
	
	// !@brief Defined in pbr_controls.cpp
	TAuto<Pipeline> create_pbr_pipeline(const DescriptorSet& set);

	// !@brief Defined in renderer.cpp
	void render_objects(VkCommandBuffer cmd);

#ifdef VALIDATION
	VkDebugUtilsMessengerEXT debugMessenger;
	const TVector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

	// !@brief Defined in debug_layers.cpp
	VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	
	// !@brief Defined in debug_layers.cpp
	void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
	
	// !@brief Defined in debug_layers.cpp
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	
	// !@brief Defined in debug_layers.cpp
	void setupDebugMessenger();
	
	// !@brief Defined in debug_layers.cpp
	VkBool32 checkValidationLayerSupport() const;
	
	// !@brief Defined in debug_layers.cpp
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
#endif
};