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

struct VulkanTexture : Texture
{
	TShared<VulkanImage> Image = VK_NULL_HANDLE;
	TAuto<VulkanImageView> View = VK_NULL_HANDLE;

public:
	void reset()
	{
		Image.reset();
		View.reset();
	}
};

namespace GR
{
	struct Camera
	{
	public:
		GRComponents::Projection Projection = {};
		GRComponents::Transform View = {};

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
private:
	TVector<const char*> m_ExtensionsList = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	TVector<VkImage> m_SwapchainImages = {};
	TVector<VkImageView> m_SwapchainViews = {};


	TVector<TAuto<VulkanImage>> m_DepthAttachmentsHR = {};
	TVector<TAuto<VulkanImageView>> m_DepthViewsHR = {};
	
	TVector<TAuto<VulkanImage>> m_HdrAttachmentsHR = {};
	TVector<TAuto<VulkanImageView>> m_HdrViewsHR = {};


	TVector<TAuto<VulkanImage>> m_HdrAttachmentsLR = {};
	TVector<TAuto<VulkanImageView>> m_HdrViewsLR = {};

	TVector<TAuto<VulkanImage>> m_DepthAttachmentsLR = {};
	TVector<TAuto<VulkanImageView>> m_DepthViewsLR = {};


	TVector<VkFramebuffer> m_FramebuffersHR = {};
	TVector<VkFramebuffer> m_FramebuffersLR = {};

	TVector<VkFence> m_PresentFences = {};
	TVector<VkSemaphore> m_PresentSemaphores = {};
	TVector<VkSemaphore> m_SwapchainSemaphores = {};
	TVector<VkCommandBuffer> m_PresentBuffers = {};
	TVector<TAuto<Pipeline>> m_HDRPipelines = {};
	TVector<TAuto<DescriptorSet>> m_HDRDescriptors = {};

	VkInstance m_VkInstance = VK_NULL_HANDLE;
	VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

	RenderScope m_Scope = {};
	GLFWwindow* m_GlfwWindow = VK_NULL_HANDLE;

	TVector<TAuto<Buffer>> m_UBOBuffers = {};
	TAuto<Buffer> m_CloudLayer = {};

	TVector<TAuto<DescriptorSet>> m_UBOSets = {};

	TAuto<GraphicsObject> m_Volumetrics = VK_NULL_HANDLE;
	TAuto<GraphicsObject> m_Atmospherics = VK_NULL_HANDLE;

	VulkanTexture m_VolumeShape = {};
	VulkanTexture m_VolumeDetail = {};

	VulkanTexture m_ScatteringLUT = {};
	VulkanTexture m_IrradianceLUT = {};
	VulkanTexture m_TransmittanceLUT = {};

	uint32_t m_SwapchainIndex = 0;

#ifdef INCLUDE_GUI
	VkDescriptorPool m_ImguiPool = VK_NULL_HANDLE;
#endif

public:
	// !@brief Defined in renderer.cpp
	VulkanBase(GLFWwindow* window, entt::registry& registry);

	// !@brief Defined in renderer.cpp
	~VulkanBase() noexcept;
	/*
	* !@brief INTERNAL. Renders the next frame of simulation. Defined in renderer.cpp.
	*/
	void _step(float DeltaTime);
	/*
	* !@brief INTERNAL. Handles recreation of swapchain dependant objects. Defined in renderer.cpp.
	*/
	void _handleResize();
	/*
	* !@brief Load mesh model to the scene. Defined in renderer.cpp.
	* 
	* @param[in] mesh_path - local path to the mesh file (for supported formats look at assimp)
	* 
	* @return New entity handle
	*/
	GRAPI entt::entity AddMesh(const std::string& mesh_path);
	/*
	* !@brief Generate and add a simple shape to the scene. Defined in renderer.cpp.
	* 
	* @param[in] descriptor - description of the shape (aka Sphere, Plane, Cube)
	* 
	* @return New entity handle
	*/
	GRAPI entt::entity AddShape(const GRShape::Shape& descriptor);
	/*
	* !@brief INTERNAL. Import image file into the memory using specified format
	* 
	* @param[in] path - path to image file
	* @param[in] format - format to store image in
	* 
	* @return Vulkan image memory
	*/
	TAuto<VulkanTexture> _loadImage(const std::string& path, VkFormat format);
	/*
	* !@brief Wait on CPU for GPU to complete it's current rendering commands
	*/
	GRAPI void Wait() const;
	/*
	* !@brief Customize volumetric clouds
	* 
	* @param[in] settings - new parameters of cloud rendering
	*/
	GRAPI void SetCloudLayerSettings(CloudLayerProfile settings);
	/*
	* !@brief Should be modified to control the scene
	*/
	GR::Camera m_Camera = {};
	TVec3 m_SunDirection = glm::normalize(TVec3(1.0));

private:
	entt::registry& m_Registry;

	TShared<VulkanTexture> m_DefaultWhite = {};
	TShared<VulkanTexture> m_DefaultBlack = {};
	TShared<VulkanTexture> m_DefaultNormal = {};
	TShared<VulkanTexture> m_DefaultARM = {};

private:

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
	TAuto<DescriptorSet> create_pbr_set(const VulkanImageView& albedo, const VulkanImageView& nh, const VulkanImageView& arm);
	
	// !@brief Defined in pbr_controls.cpp
	TAuto<Pipeline> create_pbr_pipeline(const DescriptorSet& set);

	// !@brief Defined in renderer.cpp
	void render_objects(VkCommandBuffer cmd);

#ifdef VALIDATION
	VkDebugUtilsMessengerEXT m_DebugMessenger;
	const TVector<const char*> m_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };

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