#pragma once
#include "core.hpp"
#include "entt/entt.hpp"
#include "Vulkan/graphics_object.hpp"
#include "Vulkan/scope.hpp"
#include "Vulkan/queue.hpp"
#include "Vulkan/buffer.hpp"
#include "Vulkan/image.hpp"
#include "Vulkan/mesh.hpp"
#include "Vulkan/vulkan_api.hpp"
#include "Vulkan/noise.hpp"
#include "Engine/components.hpp"
#include "Engine/structs.hpp"
#include "Engine/shapes.hpp"
#include "Engine/world.hpp"

#ifdef INCLUDE_GUI
#include "imgui/imgui.h"
#endif

#if DEBUG == 1
#define VALIDATION
#endif

#ifndef PLANET_RADIUS
#define PLANET_RADIUS 6360.0 * 1e3
#endif

#ifndef ATMOSPHERE_RADIUS
#define ATMOSPHERE_RADIUS 6420.0 * 1e3
#endif

struct VulkanTexture : Texture
{
	std::shared_ptr<VulkanImage> Image = VK_NULL_HANDLE;
	std::unique_ptr<VulkanImageView> View = VK_NULL_HANDLE;

public:
	void reset()
	{
		Image.reset();
		View.reset();
	}
};

namespace GR
{
	class Window;

	class Camera
	{
	public:
		Components::ProjectionMatrix Projection = {};
		Components::TransformMatrix<double> View = {};

		glm::dmat4 get_view_matrix() const
		{
			return glm::lookAt(View.GetOffset(), View.GetOffset() + View.GetForward(), View.GetUp());
		}

		glm::mat4 get_projection_matrix() const
		{
			return Projection.matrix;
		}

		glm::dmat4 get_view_projection() const
		{
			return glm::dmat4(Projection.matrix) * View.matrix;
		}

	private:
		friend class VulkanBase;
	};

	class Renderer
	{
	protected:
#ifdef INCLUDE_GUI
		ImGuiContext* m_GuiContext = nullptr;
#endif
	public:
		virtual ~Renderer() = default;

		virtual void BeginFrame() = 0;

		virtual void EndFrame() = 0;

		virtual void SetCloudLayerSettings(CloudLayerProfile settings) = 0;

#ifdef INCLUDE_GUI
		ImGuiContext* GetImguiContext() const { return m_GuiContext; }
#endif

	public:
		/*
		* !@brief Should be modified to control the scene
		*/
		GR::Camera m_Camera = {};
		glm::vec3 m_SunDirection = glm::normalize(glm::vec3(1.0));

		static constexpr float Rg = PLANET_RADIUS;
	};
};

class VulkanBase final : public GR::Renderer
{
private:
	friend class GR::Window;

	std::vector<const char*> m_ExtensionsList = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	std::vector<VkImage> m_SwapchainImages = {};
	std::vector<VkImageView> m_SwapchainViews = {};


	std::vector<std::unique_ptr<VulkanImage>> m_DepthAttachmentsHR = {};
	std::vector<std::unique_ptr<VulkanImageView>> m_DepthViewsHR = {};
	
	std::vector<std::unique_ptr<VulkanImage>> m_HdrAttachmentsHR = {};
	std::vector<std::unique_ptr<VulkanImageView>> m_HdrViewsHR = {};


	std::vector<std::unique_ptr<VulkanImage>> m_HdrAttachmentsLR = {};
	std::vector<std::unique_ptr<VulkanImageView>> m_HdrViewsLR = {};

	std::vector<std::unique_ptr<VulkanImage>> m_DepthAttachmentsLR = {};
	std::vector<std::unique_ptr<VulkanImageView>> m_DepthViewsLR = {};


	std::vector<VkFramebuffer> m_FramebuffersHR = {};
	std::vector<VkFramebuffer> m_FramebuffersLR = {};

	std::vector<VkFence> m_PresentFences = {};
	std::vector<VkSemaphore> m_PresentSemaphores = {};
	std::vector<VkSemaphore> m_SwapchainSemaphores = {};
	std::vector<VkCommandBuffer> m_PresentBuffers = {};
	std::vector<std::unique_ptr<Pipeline>> m_HDRPipelines = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_HDRDescriptors = {};

	VkInstance m_VkInstance = VK_NULL_HANDLE;
	VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

	RenderScope m_Scope = {};
	GLFWwindow* m_GlfwWindow = VK_NULL_HANDLE;

	std::vector<std::unique_ptr<Buffer>> m_UBOBuffers = {};
	std::unique_ptr<Buffer> m_CloudLayer = {};

	std::vector<std::unique_ptr<DescriptorSet>> m_UBOSets = {};

	std::unique_ptr<GraphicsObject> m_Volumetrics = VK_NULL_HANDLE;
	std::unique_ptr<GraphicsObject> m_Atmospherics = VK_NULL_HANDLE;

	VulkanTexture m_VolumeShape = {};
	VulkanTexture m_VolumeDetail = {};
	VulkanTexture m_VolumeWeather = {};

	VulkanTexture m_ScatteringLUT = {};
	VulkanTexture m_IrradianceLUT = {};
	VulkanTexture m_TransmittanceLUT = {};

	uint32_t m_SwapchainIndex = 0;

#ifdef INCLUDE_GUI
	VkDescriptorPool m_ImguiPool = VK_NULL_HANDLE;
#endif

	std::shared_ptr<VulkanTexture> m_DefaultWhite = {};
	std::shared_ptr<VulkanTexture> m_DefaultBlack = {};
	std::shared_ptr<VulkanTexture> m_DefaultNormal = {};
	std::shared_ptr<VulkanTexture> m_DefaultARM = {};

#if DEBUG == 1
	bool m_InFrame = false;
#endif

public:
	VulkanBase(GLFWwindow* window);

	~VulkanBase() noexcept;
	/*
	* !@brief Renders the next frame of simulation. Defined in renderer.cpp.
	*/
	GRAPI void BeginFrame() override;
	/*
	*
	*/
	GRAPI void EndFrame() override;
	/*
	* !@brief Handles recreation of swapchain dependant objects. Defined in renderer.cpp.
	*/
	void _handleResize();
	/*
	* !@brief Wait on CPU for GPU to complete it's current rendering commands
	*/
	GRAPI void Wait() const;
	/*
	* !@brief Customize volumetric clouds
	* 
	* @param[in] settings - new parameters of cloud rendering
	*/
	GRAPI void SetCloudLayerSettings(CloudLayerProfile settings) override;
	/*
	* !@brief INTERNAL. Import image file into the memory using specified format
	*
	* @param[in] path - path to image file
	* @param[in] format - format to store image in
	*
	* @return Vulkan image memory
	*/
	std::unique_ptr<VulkanTexture> _loadImage(const std::string& path, VkFormat format) const;
	/*
	* 
	*/
	entt::entity _constructShape(entt::entity ent, entt::registry& registry, const GR::Shapes::Shape& shape) const;
	/*
	*
	*/
	void _drawObject(const PBRObject& gro, const PBRConstants& constants) const;
	/*
	*
	*/
	void _updateObject(entt::entity ent, entt::registry& registry) const;

private:

	VkBool32 create_instance();

	VkBool32 create_swapchain_images();

	VkBool32 create_framebuffers();

	VkBool32 create_hdr_pipeline();

	VkBool32 prepare_renderer_resources();

	std::vector<const char*> getRequiredExtensions();

	VkBool32 atmosphere_precompute();

	VkBool32 volumetric_precompute();

	std::unique_ptr<DescriptorSet> create_pbr_set(const VulkanImageView& albedo, const VulkanImageView& nh, const VulkanImageView& arm) const;
	
	std::unique_ptr<Pipeline> create_pbr_pipeline(const DescriptorSet& set) const;

#ifdef VALIDATION
	VkDebugUtilsMessengerEXT m_DebugMessenger;
	const std::vector<const char*> m_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };

	VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	
	void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
	
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	
	void setupDebugMessenger();
	
	VkBool32 checkValidationLayerSupport() const;
	
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
#endif
};