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
#ifndef VALIDATION_OFF
#define VALIDATION
#endif
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

struct VulkanTextureMultiView : Texture
{
	std::shared_ptr<VulkanImage> Image = VK_NULL_HANDLE;
	std::vector<std::unique_ptr<VulkanImageView>> Views = {};

public:
	void reset()
	{
		Image.reset();
		Views.resize(0);
	}
};

class VulkanBase;

namespace GR
{
	class Window;

	class Camera
	{
	public:
		glm::dmat4 GetViewMatrix() const
		{
			return glm::lookAt(Transform.GetOffset(), Transform.GetOffset() + glm::dvec3(Transform.GetForward()), glm::dvec3(Transform.GetUp()));
		}

		glm::mat4 GetProjectionMatrix() const
		{
			return Projection.matrix;
		}

		glm::mat4 GetInfiniteProjectionMatrix() const
		{
			glm::mat4 matrix = Projection.matrix;
			matrix[2][2] = 0.0;
			matrix[3][2] = -1.0;
			return matrix;
		}

		glm::dmat4 GetViewProjection() const
		{
			return glm::dmat4(GetProjectionMatrix()) * GetViewMatrix();
		}

		glm::vec4* GetFrustumPlanes() const
		{
			return (glm::vec4*)_planes;
		}

		bool FrustumCull(glm::dmat4 worldmatrix, glm::vec3 Min, glm::vec3 Max)
		{
			glm::vec4 points[8] = {
				worldmatrix * glm::vec4(Min.x, Min.y, Min.z, 1.0),
				worldmatrix * glm::vec4(Max.x, Max.y, Max.z, 1.0),
				worldmatrix * glm::vec4(Max.x, Min.y, Min.z, 1.0),
				worldmatrix * glm::vec4(Min.x, Max.y, Min.z, 1.0),
				worldmatrix * glm::vec4(Min.x, Min.y, Max.z, 1.0),
				worldmatrix * glm::vec4(Max.x, Max.y, Min.z, 1.0),
				worldmatrix * glm::vec4(Min.x, Max.y, Max.z, 1.0),
				worldmatrix * glm::vec4(Max.x, Min.y, Max.z, 1.0)
			};

			for (uint32_t i = 0; i < 6; i++)
			{
				if (glm::dot(_planes[i], points[0]) < 0.0
					&& glm::dot(_planes[i], points[1]) < 0.0
					&& glm::dot(_planes[i], points[2]) < 0.0
					&& glm::dot(_planes[i], points[3]) < 0.0
					&& glm::dot(_planes[i], points[4]) < 0.0
					&& glm::dot(_planes[i], points[5]) < 0.0
					&& glm::dot(_planes[i], points[6]) < 0.0
					&& glm::dot(_planes[i], points[7]) < 0.0)
				{
					return false;
				}
			}

			return true;
		}

		bool FrustumCull(glm::dmat4 worldmatrix, glm::vec3 Point)
		{
			glm::vec4 point = worldmatrix * glm::vec4(Point, 1.0);

			for (uint32_t i = 0; i < 6; i++)
			{
				if (glm::dot(_planes[0], point) < 0.0
					&& glm::dot(_planes[1], point) < 0.0
					&& glm::dot(_planes[2], point) < 0.0
					&& glm::dot(_planes[3], point) < 0.0
					&& glm::dot(_planes[4], point) < 0.0
					&& glm::dot(_planes[5], point) < 0.0)
				{
					return false;
				}
			}

			return true;
		}

	private:
		void build_frustum_planes(glm::dmat4 view_projection)
		{
			for (int i = 4; i--; ) { _planes[0][i] = view_projection[i][3] + view_projection[i][0]; }
			for (int i = 4; i--; ) { _planes[1][i] = view_projection[i][3] - view_projection[i][0]; }
			for (int i = 4; i--; ) { _planes[2][i] = view_projection[i][3] + view_projection[i][1]; }
			for (int i = 4; i--; ) { _planes[3][i] = view_projection[i][3] - view_projection[i][1]; }
			for (int i = 4; i--; ) { _planes[4][i] = view_projection[i][3] + view_projection[i][2]; }
			for (int i = 4; i--; ) { _planes[5][i] = view_projection[i][3] - view_projection[i][2]; }
		}

	public:
		Components::ProjectionMatrix Projection = {};
		Components::WorldMatrix Transform = {};

		float Exposure = 1.0;
		float Gamma = 2.2;

	private:
		friend class VulkanBase;
		glm::vec4 _planes[6];
	};

	class Renderer
	{
	protected:
#ifdef INCLUDE_GUI
		ImGuiContext* m_GuiContext = nullptr;
#endif
	public:
		virtual ~Renderer() = default;

		virtual bool BeginFrame() = 0;

		virtual void EndFrame() = 0;

		virtual void SetCloudLayerSettings(CloudLayerProfile settings) = 0;

		virtual void SetTerrainLayerSettings(float Scale, int Count, TerrainLayerProfile* settings) = 0;

#ifdef INCLUDE_GUI
		ImGuiContext* GetImguiContext() const { return m_GuiContext; }
#endif

	public:
		/*
		* !@brief Should be modified to control the scene
		*/
		GR::Camera m_Camera = {};
		glm::vec3 m_SunDirection = glm::normalize(glm::vec3(1.0));
		float WindSpeed = 0.25;

#ifndef PLANET_RADIUS
		static constexpr float Rg = 6360.0 * 1e3;
#else
		static constexpr float Rg = PLANET_RADIUS;
#endif

#ifndef ATMOSPHERE_RADIUS
		static constexpr float Rt = 6420.0 * 1e3;
#else
		static constexpr float Rt = ATMOSPHERE_RADIUS;
#endif

		static constexpr float Rcb = Rg + 0.25 * (Rt - Rg);
		static constexpr float Rct = Rg + 0.95 * (Rt - Rg);

	protected:
		float Rcbb;
		float Rctb;
	};
};

class VulkanBase final : public GR::Renderer
{
private:

	struct CloudParameters
	{
		float Coverage;
		float CoverageSq;
		float HeightFactor;
		float BottomSmoothnessFactor;
		float LightIntensity;
		float Ambient;
		float Density;
		float BottomBound;
		float TopBound;
		float BoundDelta;
	} cloudParams;

	const uint32_t LRr = 2;
	const uint32_t CubeR = 128;

	friend class GR::Window;

	std::vector<const char*> m_ExtensionsList = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME };

	std::vector<VulkanSynchronization> m_ApplySync = {};
	std::vector<VulkanSynchronization> m_PresentSync = {};
	std::vector<VulkanSynchronization> m_ComposeSync = {};
	std::vector<VulkanSynchronization> m_DeferredSync = {};
	std::vector<VulkanSynchronization> m_TerrainAsync = {};
	std::vector<VulkanSynchronization> m_CubemapAsync = {};
	std::vector<VulkanSynchronization> m_BackgroundAsync = {};
	/*
	* Frame resources
	*/
	std::vector<VkImage> m_SwapchainImages = {};
	std::vector<VkImageView> m_SwapchainViews = {};

	std::vector<VulkanTextureMultiView> m_DepthHR = {};
	
	// std::vector<std::unique_ptr<VulkanImage>> m_DepthAttachmentsHR = {};
	// std::vector<std::unique_ptr<VulkanImageView>> m_DepthViewsHR = {};
	
	std::vector<std::unique_ptr<VulkanImage>> m_HdrAttachmentsHR = {};
	std::vector<std::unique_ptr<VulkanImageView>> m_HdrViewsHR = {};

	std::vector<std::unique_ptr<VulkanImage>> m_DeferredAttachments = {};
	std::vector<std::unique_ptr<VulkanImageView>> m_DeferredViews = {};

	std::vector<std::unique_ptr<VulkanImage>> m_BlurAttachments = {};
	std::vector<std::unique_ptr<VulkanImageView>> m_BlurViews = {};

	std::vector<std::unique_ptr<VulkanImage>> m_NormalAttachments = {};
	std::vector<std::unique_ptr<VulkanImageView>> m_NormalViews = {};

	std::vector<std::unique_ptr<VulkanImage>> m_HdrAttachmentsLR = {};
	std::vector<std::unique_ptr<VulkanImageView>> m_HdrViewsLR = {};

	std::vector<std::unique_ptr<VulkanImage>> m_DepthAttachmentsLR = {};
	std::vector<std::unique_ptr<VulkanImageView>> m_DepthViewsLR = {};

	// std::vector<VulkanTexture> m_DepthCopies = {};

	std::vector<VkFramebuffer> m_FramebuffersHR   = {};
	std::vector<VkFramebuffer> m_FramebuffersTR   = {};
	std::vector<VkFramebuffer> m_FramebuffersCP   = {};
	std::vector<VkFramebuffer> m_FramebuffersPP   = {};

	std::unique_ptr<GraphicsPipeline> m_CompositionPipeline = VK_NULL_HANDLE;
	std::unique_ptr<GraphicsPipeline> m_PostProcessPipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_BlendingPipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_BlurSetupPipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_BlurHorizontalPipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_BlurVerticalPipeline = VK_NULL_HANDLE;

	std::vector<std::unique_ptr<DescriptorSet>> m_SubpassDescriptors = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_BlendingDescriptors = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_CompositionDescriptors = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_BlurDescriptors = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_PostProcessDescriptors = {};

	std::vector<VkSemaphore> m_SwapchainSemaphores = {};

	std::vector<uint32_t> m_ImageIndex = {};
	uint32_t m_ResourceIndex = 0;
	uint32_t m_ResourceCount = 0;
	uint64_t m_FrameCount = 0;

	std::vector<VkSubmitInfo> m_GraphicsSubmits;
	std::vector<VkFence> m_GraphicsFences;
	VkFence m_AcquireFence;
	/*
	* Volumetrics resources
	*/
	std::vector<std::unique_ptr<DescriptorSet>> m_TemporalVolumetrics = {};

	std::unique_ptr<DescriptorSet> m_VolumetricsDescriptor = VK_NULL_HANDLE;
	std::vector<std::unique_ptr<DescriptorSet>> m_UBOSkySets = {};

	std::vector<std::unique_ptr<Buffer>> m_UBOSkyBuffers = {};
	std::unique_ptr<Buffer> m_CloudLayer = {};

	std::unique_ptr<ComputePipeline> m_VolumetricsAbovePipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_VolumetricsBetweenPipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_VolumetricsUnderPipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_VolumetricsComposePipeline = VK_NULL_HANDLE;

	VulkanTexture m_VolumeShape = {};
	VulkanTexture m_VolumeDetail = {};
	VulkanTexture m_VolumeWeather = {};
	VulkanTexture m_VolumeWeatherCube = {};
	/*
	* Atmosphere resources
	*/
	VulkanTexture m_ScatteringLUT = {};
	VulkanTexture m_IrradianceLUT = {};
	VulkanTexture m_TransmittanceLUT = {};
	/*
	* PBR resources
	*/
	std::unique_ptr<ComputePipeline> m_CubemapPipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_CubemapMipPipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_ConvolutionPipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_SpecularIBLPipeline = VK_NULL_HANDLE;

	std::vector<std::unique_ptr<DescriptorSet>> m_CubemapDescriptors = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_CubemapMipDescriptors = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_ConvolutionDescriptors = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_SpecularDescriptors = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_DiffuseDescriptors = {};

	std::unique_ptr<Buffer> m_DiffusePrecompute = VK_NULL_HANDLE;
	std::unique_ptr<Buffer> m_SpecularPrecompute = VK_NULL_HANDLE;

	VulkanTexture m_BRDFLUT = {};
	std::vector<VulkanTexture> m_DiffuseIrradience = {};
	std::vector<VulkanTextureMultiView> m_CubemapLUT = {};
	std::vector<VulkanTextureMultiView> m_SpecularLUT = {};
	/*
	* Terrain resources
	*/
	std::unique_ptr<GraphicsPipeline> m_TerrainTexturingPipeline = VK_NULL_HANDLE;
	std::unique_ptr<ComputePipeline> m_TerrainCompute = {};
	std::unique_ptr<ComputePipeline> m_TerrainCompose = {};
	std::unique_ptr<ComputePipeline> m_GrassOcclude   = {};
	std::unique_ptr<GraphicsPipeline> m_GrassPipeline = {};

	std::unique_ptr<Buffer> m_TerrainLayer = {};
	std::unique_ptr<Buffer> m_GrassIndirectRef = {};
	std::vector<std::unique_ptr<Buffer>> m_GrassIndirect  = {};
	std::vector<std::unique_ptr<Buffer>> m_GrassPositions = {};
	std::vector<std::unique_ptr<Buffer>> TerrainVBs = {};

	std::vector<VulkanTexture> m_TerrainLUT = {};

	std::vector<std::unique_ptr<DescriptorSet>> m_TerrainSet = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_TerrainDrawSet = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_GrassSet = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_GrassDrawSet = {};

	uint32_t m_TerrainDispatches = 0u;
	/*
	* Common
	*/
	std::vector<std::unique_ptr<Buffer>> m_UBOTempBuffers = {};
	std::vector<std::unique_ptr<Buffer>> m_UBOBuffers = {};

	std::vector<std::unique_ptr<DescriptorSet>> m_UBOSets = {};
	std::vector<std::unique_ptr<DescriptorSet>> m_UBOTempSets = {};

	VkInstance m_VkInstance = VK_NULL_HANDLE;
	VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

	RenderScope m_Scope = {};
	GLFWwindow* m_GlfwWindow = VK_NULL_HANDLE;

#ifdef INCLUDE_GUI
	VkDescriptorPool m_ImguiPool = VK_NULL_HANDLE;
#endif

	std::shared_ptr<VulkanTextureMultiView> m_DefaultWhite = {};
	std::shared_ptr<VulkanTextureMultiView> m_DefaultBlack = {};
	std::shared_ptr<VulkanTextureMultiView> m_DefaultNormal = {};
	std::shared_ptr<VulkanTextureMultiView> m_DefaultARM = {};

#if DEBUG == 1
	bool m_InFrame = false;
#endif

public:
	VulkanBase(GLFWwindow* window);

	~VulkanBase() noexcept;
	/*
	* !@brief Renders the next frame of simulation. Defined in renderer.cpp.
	*/
	GRAPI bool BeginFrame() override;
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

	GRAPI void SetTerrainLayerSettings(float Scale, int Count, TerrainLayerProfile* settings) override;
	/*
	* !@brief INTERNAL. Import image file into the memory using specified format
	*
	* @param[in] path - path to image file
	* @param[in] format - format to store image in
	*
	* @return Vulkan image memory
	*/
	std::unique_ptr<VulkanTexture> _loadImage(const std::vector<std::string>& path, VkFormat format) const;
	/*
	* 
	*/
	entt::entity _constructShape(entt::entity ent, entt::registry& registry, const GR::Shapes::GeoClipmap& shape);
	/*
	* 
	*/
	entt::entity _constructShape(entt::entity ent, entt::registry& registry, const GR::Shapes::Shape& shape, GR::Shapes::GeometryDescriptor* geometry);
	/*
	* 
	*/
	void _drawObject(const PBRObject& gro, const PBRConstants& constants) const;
	/*
	*
	*/
	void _drawTerrain(const PBRObject& gro, const PBRConstants& constants) const;
	/*
	* 
	*/
	void _updateObject(entt::entity ent, entt::registry& registry) const;
	/*
	*
	*/
	void _updateTerrain(entt::entity ent, entt::registry& registry) const;
	/*
	* 
	*/
	void _beginTerrainPass() const;

private:
	VkBool32 create_instance();

	VkBool32 create_swapchain_images();

	VkBool32 create_framebuffers();

	VkBool32 create_frame_pipelines();

	VkBool32 create_frame_descriptors();

	VkBool32 prepare_renderer_resources();

	std::vector<const char*> getRequiredExtensions();

	VkBool32 atmosphere_precompute();

	VkBool32 volumetric_precompute();

	VkBool32 brdf_precompute();

	VkBool32 terrain_init(const Buffer& VB, const GR::Shapes::GeoClipmap& shape);

	std::unique_ptr<DescriptorSet> create_pbr_set(const VulkanImageView& albedo, const VulkanImageView& nh, const VulkanImageView& arm) const;
	
	std::unique_ptr<GraphicsPipeline> create_pbr_pipeline(const DescriptorSet& set) const;

	std::unique_ptr<DescriptorSet> create_terrain_set(const VulkanImageView& albedo, const VulkanImageView& nh, const VulkanImageView& arm) const;

	std::unique_ptr<GraphicsPipeline> create_terrain_pipeline(const DescriptorSet& set, const GR::Shapes::GeoClipmap& shape) const;

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