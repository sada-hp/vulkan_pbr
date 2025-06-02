#define VK_KHR_swapchain 
#define VMA_IMPLEMENTATION
#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION

#include "pch.hpp"
#include "renderer.hpp"
#include "Engine/utils.hpp"
#include <stb/stb_image.h>

#ifdef INCLUDE_GUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"
#endif

#define WRAPL(i) (i == 0 ? m_ResourceCount : i) - 1
#define WRAPR(i) i == m_ResourceCount - 1 ? 0 : i + 1

#pragma region Utils
std::unique_ptr<VulkanImage> create_image(const RenderScope& Scope, void* pixels, int count, int w, int h, int c, const VkFormat& format, const VkImageCreateFlags& flags)
{
	assert(count > 0 && w > 0 && h > 0 && c > 0);

	int resolution = w * h * c;
	uint32_t familyIndex = Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex();

	VkBufferCreateInfo sbInfo{};
	sbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sbInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	sbInfo.queueFamilyIndexCount = 1;
	sbInfo.pQueueFamilyIndices = &familyIndex;
	sbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sbInfo.size = resolution * count;

	VmaAllocationCreateInfo sbAlloc{};
	sbAlloc.usage = VMA_MEMORY_USAGE_AUTO;
	sbAlloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	
	Buffer stagingBuffer(Scope, sbInfo, sbAlloc);

	if (pixels)
		stagingBuffer.Update(pixels);

	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;
	VkImageSubresourceRange subRes{};
	subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subRes.baseArrayLayer = 0;
	subRes.baseMipLevel = 0;
	subRes.layerCount = count;
	subRes.levelCount = mipLevels;
	
	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.arrayLayers = count;
	imageCI.extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1u };
	imageCI.format = format;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCI.mipLevels = mipLevels;
	imageCI.flags = flags;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageCI.imageType = VK_IMAGE_TYPE_2D;

	VmaAllocationCreateInfo skyAlloc{};
	skyAlloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(Scope.GetPhysicalDevice(), &properties);
	
	std::unique_ptr<VulkanImage> target = std::make_unique<VulkanImage>(Scope);
	target->CreateImage(imageCI, skyAlloc);

	VkCommandBuffer cmd;
	if (pixels)
	{
		Scope.GetQueue(VK_QUEUE_TRANSFER_BIT)
			.AllocateCommandBuffers(1, &cmd);

		BeginOneTimeSubmitCmd(cmd);
		target->TransitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_TRANSFER_BIT);
		CopyBufferToImage(cmd, target->GetImage(), stagingBuffer.GetBuffer(), subRes, imageCI.extent);
		EndCommandBuffer(cmd);

		Scope.GetQueue(VK_QUEUE_TRANSFER_BIT)
			.Submit(cmd)
			.Wait()
			.FreeCommandBuffers(1, &cmd);
	}

	Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers(1, &cmd);

	BeginOneTimeSubmitCmd(cmd);
	target->GenerateMipMaps(cmd);
	EndCommandBuffer(cmd);

	Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	return target;
}
#pragma endregion

VulkanBase::VulkanBase(GLFWwindow* window)
	: m_GlfwWindow(window)
{
	VkPhysicalDeviceVulkan12Features featureVk12{};
	featureVk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	featureVk12.drawIndirectCount = VK_TRUE;

	VkPhysicalDeviceShaderAtomicFloatFeaturesEXT featureFloats{};
	featureFloats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
	featureFloats.shaderImageFloat32AtomicAdd = VK_TRUE;
	featureFloats.shaderImageFloat32Atomics = VK_TRUE;
	featureFloats.pNext = &featureVk12;

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.imageCubeArray = VK_TRUE;
	deviceFeatures.fullDrawIndexUint32 = VK_TRUE;
	deviceFeatures.shaderFloat64 = VK_TRUE;
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.independentBlend = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;
	deviceFeatures.geometryShader = VK_TRUE;
	deviceFeatures.multiDrawIndirect = VK_TRUE;

	std::vector<VkDescriptorPoolSize> pool_sizes =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkBool32 res = create_instance();
	
	res = (glfwCreateWindowSurface(m_VkInstance, m_GlfwWindow, VK_NULL_HANDLE, &m_Surface) == VK_SUCCESS) & res;

	m_Scope.CreatePhysicalDevice(m_VkInstance, m_ExtensionsList)
		.CreateLogicalDevice(deviceFeatures, m_ExtensionsList, { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_COMPUTE_BIT }, &featureFloats)
		.CreateMemoryAllocator(m_VkInstance)
		.CreateSwapchain(m_Surface)
		.CreateDefaultRenderPass()
		.CreateLowResRenderPass()
		.CreateCompositionRenderPass()
		.CreatePostProcessRenderPass()
		.CreateCubemapRenderPass()
		.CreateSimpleRenderPass()
		.CreateTerrainRenderPass()
		.CreateDescriptorPool(1000u, pool_sizes);
	
	res = create_swapchain_images() & res;
	res = create_framebuffers() & res;

	m_Camera.Projection.SetAspect(static_cast<float>(m_Scope.GetSwapchainExtent().width) / static_cast<float>(m_Scope.GetSwapchainExtent().height))
		.SetFOV(glm::radians(45.f))
		.SetDepthRange(1e-2f, 1e4f);

	m_ImageIndex.resize(m_ResourceCount);
	m_SwapchainSemaphores.resize(m_ResourceCount);

	m_ApplySync.resize(m_ResourceCount);
	m_ComposeSync.resize(m_ResourceCount);
	m_PresentSync.resize(m_ResourceCount);
	m_CubemapAsync.resize(m_ResourceCount);
	m_DeferredSync.resize(m_ResourceCount);
	m_TerrainAsync.resize(m_ResourceCount);
	m_BackgroundAsync.resize(m_ResourceCount);

	m_GraphicsFences.resize(m_ResourceCount);

	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_ApplySync.size(), 2, m_ApplySync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_PresentSync.size(), 2, m_PresentSync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetPool(), m_TerrainAsync.size(), 1, m_TerrainAsync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetPool(), m_CubemapAsync.size(), 1, m_CubemapAsync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_DeferredSync.size(), 1, m_DeferredSync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_ComposeSync.size(), 1, m_ComposeSync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetPool(), m_BackgroundAsync.size(), 1, m_BackgroundAsync.data());

	std::for_each(m_SwapchainSemaphores.begin(), m_SwapchainSemaphores.end(), [&, this](VkSemaphore& it) {
		res = CreateSemaphore(m_Scope.GetDevice(), &it) & res;
	});

	std::for_each(m_GraphicsFences.begin(), m_GraphicsFences.end(), [&, this](VkFence& it) {
		res = CreateFence(m_Scope.GetDevice(), &it, VK_TRUE) & res;
	});

	res = CreateFence(m_Scope.GetDevice(), &m_AcquireFence, VK_FALSE) & res;

	res = prepare_renderer_resources() & res;
	res = atmosphere_precompute() & res;
	res = volumetric_precompute() & res;
	res = brdf_precompute() & res;
	res = create_frame_descriptors() & res;
	res = create_frame_pipelines() & res;

#ifdef INCLUDE_GUI
	m_GuiContext = ImGui::CreateContext();
	ImGui::SetCurrentContext(m_GuiContext);
	ImGui_ImplGlfw_InitForVulkan(m_GlfwWindow, false);

	::CreateDescriptorPool(m_Scope.GetDevice(), pool_sizes.data(), pool_sizes.size(), 1000, &m_ImguiPool);

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_VkInstance;
	init_info.PhysicalDevice = m_Scope.GetPhysicalDevice();
	init_info.Device = m_Scope.GetDevice();
	init_info.Queue = m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue();
	init_info.DescriptorPool = m_ImguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.RenderPass = m_Scope.GetPostProcessPass();
	init_info.Subpass = 1;

	ImGui_ImplVulkan_Init(&init_info);
#endif

	m_Camera.Transform.SetOffset(0.0f, GR::Renderer::Rg, 0.0f);

	assert(res != 0);
}

VulkanBase::~VulkanBase() noexcept
{
	vkDeviceWaitIdle(m_Scope.GetDevice());

#ifdef INCLUDE_GUI
	ImGui_ImplVulkan_Shutdown();
	vkDestroyDescriptorPool(m_Scope.GetDevice(), m_ImguiPool, VK_NULL_HANDLE);
#endif

	m_TerrainLayer.reset();

	m_GrassSet.resize(0);
	m_GrassPipeline.reset();

	m_VolumeWeatherCube.reset();

	m_DiffusePrecompute.reset();
	m_SpecularPrecompute.reset();

	m_GrassOcclude.reset();
	m_GrassIndirect.resize(0);
	m_GrassIndirectRef.reset();
	m_GrassPositions.resize(0);
	m_GrassDrawSet.resize(0);

	m_TerrainCompute.reset();
	m_TerrainCompose.reset();
	m_BRDFLUT.reset();

	m_TerrainDrawSet.resize(0);
	m_TerrainSet.resize(0);
	m_TerrainLUT.resize(0);
	m_DiffuseIrradience.resize(0);
	m_SpecularLUT.resize(0);
	m_CubemapLUT.resize(0);

	m_VolumetricsAbovePipeline.reset();
	m_VolumetricsUnderPipeline.reset();
	m_VolumetricsComposePipeline.reset();
	m_VolumetricsBetweenPipeline.reset();
	m_VolumetricsDescriptor.reset();
	m_UBOTempBuffers.resize(0);
	m_UBOSkyBuffers.resize(0);
	m_UBOBuffers.resize(0);

	m_TerrainTexturingPipeline.reset();
	m_SubpassDescriptors.resize(0);

	m_CloudLayer.reset();
	m_VolumeShape.reset();
	m_VolumeDetail.reset();
	m_VolumeWeather.reset();

	m_TransmittanceLUT.reset();
	m_ScatteringLUT.reset();
	m_IrradianceLUT.reset();

	m_DefaultWhite.reset();
	m_DefaultBlack.reset();
	m_DefaultNormal.reset();
	m_DefaultARM.reset();

	m_BlendingPipeline.reset();
	m_BlendingDescriptors.resize(0);

	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_ApplySync.size(), m_ApplySync.data());
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_PresentSync.size(), m_PresentSync.data());
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetPool(), m_TerrainAsync.size(), m_TerrainAsync.data());
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetPool(), m_CubemapAsync.size(), m_CubemapAsync.data());
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_DeferredSync.size(), m_DeferredSync.data());
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_ComposeSync.size(), m_ComposeSync.data());
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetPool(), m_BackgroundAsync.size(), m_BackgroundAsync.data());

	std::erase_if(m_SwapchainSemaphores, [&, this](VkSemaphore& it) {
		vkDestroySemaphore(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_GraphicsFences, [&, this](VkFence& it) {
		vkDestroyFence(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});	
	
	vkDestroyFence(m_Scope.GetDevice(), m_AcquireFence, VK_NULL_HANDLE);

	std::erase_if(m_FramebuffersHR, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersTR, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersCP, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersPP, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_SwapchainViews, [&, this](VkImageView& view) {
		vkDestroyImageView(m_Scope.GetDevice(), view, VK_NULL_HANDLE);
		return true;
	});

	m_BlurSetupPipeline.reset();
	m_BlurHorizontalPipeline.reset();
	m_BlurVerticalPipeline.reset();

	m_CubemapPipeline.reset();
	m_CubemapMipPipeline.reset();
	m_CompositionPipeline.reset();
	m_PostProcessPipeline.reset();
	m_ConvolutionPipeline.reset();
	m_SpecularIBLPipeline.reset();

	m_DepthHR.resize(0);

	m_NormalAttachments.resize(0);
	m_NormalViews.resize(0);

	TerrainVBs.resize(0);

	m_DeferredAttachments.resize(0);
	m_DeferredViews.resize(0);

	m_BlurAttachments.resize(0);
	m_BlurViews.resize(0);

	m_HdrAttachmentsHR.resize(0);
	m_HdrViewsHR.resize(0);

	m_HdrAttachmentsLR.resize(0);
	m_HdrViewsLR.resize(0);

	m_DepthAttachmentsLR.resize(0);
	m_DepthViewsLR.resize(0);

	m_CompositionDescriptors.resize(0);
	m_PostProcessDescriptors.resize(0);
	m_CubemapDescriptors.resize(0);
	m_ConvolutionDescriptors.resize(0);
	m_BlurDescriptors.resize(0);
	m_SpecularDescriptors.resize(0);
	m_DiffuseDescriptors.resize(0);
	m_CubemapMipDescriptors.resize(0);

	m_TemporalVolumetrics.resize(0);
	m_UBOTempSets.resize(0);
	m_UBOSkySets.resize(0);
	m_UBOSets.resize(0);

	m_Scope.Destroy();

	vkDestroySurfaceKHR(m_VkInstance, m_Surface, VK_NULL_HANDLE);
	vkDestroyInstance(m_VkInstance, VK_NULL_HANDLE);
}

bool VulkanBase::BeginFrame()
{
	m_GraphicsSubmits.resize(0);
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0 || glfwWindowShouldClose(m_GlfwWindow))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return false;
	}

	assert(!m_InFrame, "Finish the frame in progress first!");

	// Udpate UBO
	{
		glm::dmat4 view_matrix = m_Camera.GetViewMatrix();
		glm::dmat4 view_proj_matrix = m_Camera.GetViewProjection();
		glm::mat4 projection_matrix = m_Camera.GetProjectionMatrix();
		glm::dmat4 view_matrix_inverse = glm::inverse(view_matrix);
		glm::dmat4 reprojection_matrix = view_matrix_inverse * glm::inverse(glm::dmat4(m_Camera.GetInfiniteProjectionMatrix()));
		glm::mat4 projection_matrix_inverse = glm::inverse(projection_matrix);
		glm::dmat4 view_proj_matrix_inverse = view_matrix_inverse * static_cast<glm::dmat4>(projection_matrix_inverse);
		glm::dvec4 CameraPositionFP64 = glm::dvec4(m_Camera.Transform.GetOffset(), 1.0);
		glm::vec4 CameraPosition = glm::vec4(m_Camera.Transform.GetOffset(), 1.0);
		glm::vec4 Sun = glm::vec4(glm::normalize(m_SunDirection), 0.0);
		glm::vec4 WorldUp = glm::vec4(glm::normalize(glm::round(glm::vec3(CameraPositionFP64))), 0.0);
		glm::vec4 CameraUp = glm::vec4(m_Camera.Transform.GetUp(), 0.0);
		glm::vec4 CameraRight = glm::vec4(m_Camera.Transform.GetRight(), 0.0);
		glm::vec4 CameraForward = glm::vec4(m_Camera.Transform.GetForward(), 0.0);
		glm::vec2 ScreenSize = glm::vec2(static_cast<float>(m_Scope.GetSwapchainExtent().width), static_cast<float>(m_Scope.GetSwapchainExtent().height));
		glm::mat3 WorldOrientation = glm::mat3_cast(GR::Utils::OrientationFromNormal(glm::vec3(WorldUp)));

		m_Camera.build_frustum_planes(view_proj_matrix);

		double CameraRadius = glm::length(CameraPositionFP64);
		float Time = glfwGetTime();

		UniformBuffer Uniform
		{
			reprojection_matrix,
			view_proj_matrix,
			view_matrix,
			view_matrix_inverse,
			view_proj_matrix_inverse,
			CameraPositionFP64,
			glm::mat4(WorldOrientation),
			projection_matrix,
			projection_matrix_inverse,
			CameraPosition,
			Sun,
			WorldUp,
			CameraUp,
			CameraRight,
			CameraForward,
			ScreenSize,
			CameraRadius,
			WindSpeed,
			Time,
			m_Camera.Gamma,
			m_Camera.Exposure
		};
		memcpy(Uniform.FrustumPlanes, m_Camera._planes, sizeof(glm::vec4) * 6);

		m_UBOTempBuffers[m_ResourceIndex]->Update(static_cast<void*>(&Uniform), sizeof(Uniform));
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// Start async compute to update terrain height
	if (m_TerrainCompute.get())
	{
		vkWaitForFences(m_Scope.GetDevice(), 1, &m_TerrainAsync[m_ResourceIndex].Fence, VK_TRUE, UINT64_MAX);
		vkResetFences(m_Scope.GetDevice(), 1, &m_TerrainAsync[m_ResourceIndex].Fence);
		vkBeginCommandBuffer(m_TerrainAsync[m_ResourceIndex].Commands, &beginInfo);

		// Transfer to async queue
		if (m_FrameCount > 1)
		{
			m_TerrainLUT[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_TerrainAsync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
			// m_WaterLUT[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_TerrainAsync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
		}

		if (m_GrassIndirectRef)
		{
			VkBufferCopy region{};
			region.size = m_GrassIndirectRef->GetSize() - sizeof(VkDrawIndirectCommand);
			region.srcOffset = sizeof(VkDrawIndirectCommand);
			vkCmdCopyBuffer(m_TerrainAsync[m_ResourceIndex].Commands, m_GrassIndirectRef->GetBuffer(), m_GrassIndirect[m_ResourceIndex]->GetBuffer(), 1, &region);
		}

		m_TerrainLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
		// m_WaterLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		m_UBOTempSets[m_ResourceIndex]->BindSet(0, m_TerrainAsync[m_ResourceIndex].Commands, *m_TerrainCompute);
		m_TerrainSet[m_ResourceIndex]->BindSet(1, m_TerrainAsync[m_ResourceIndex].Commands, *m_TerrainCompute);
		m_TerrainCompute->BindPipeline(m_TerrainAsync[m_ResourceIndex].Commands);
		vkCmdDispatch(m_TerrainAsync[m_ResourceIndex].Commands, m_TerrainDispatches, 1, 1);

		m_TerrainSet[m_ResourceIndex]->BindSet(0, m_TerrainAsync[m_ResourceIndex].Commands, *m_TerrainCompose);
		m_TerrainCompose->BindPipeline(m_TerrainAsync[m_ResourceIndex].Commands);

		int i = 0;
		m_TerrainCompose->PushConstants(m_TerrainAsync[m_ResourceIndex].Commands, &i, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		vkCmdDispatch(m_TerrainAsync[m_ResourceIndex].Commands, ceil(float((m_TerrainLUT[0].Image->GetExtent().width) / 8.f)), ceil(float((m_TerrainLUT[0].Image->GetExtent().height) / 4.f)), 1);

		for (i = 1; i < m_TerrainLUT[0].Image->GetArrayLayers(); i++)
		{
			m_TerrainLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i - 1, 1), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
			m_TerrainCompose->PushConstants(m_TerrainAsync[m_ResourceIndex].Commands, &i, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
			vkCmdDispatch(m_TerrainAsync[m_ResourceIndex].Commands, ceil(float((m_TerrainLUT[0].Image->GetExtent().width) / 16.f)), ceil(float((m_TerrainLUT[0].Image->GetExtent().height) / 8.f)), 1);
		}
		m_TerrainLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		if (m_GrassOcclude)
		{
			m_GrassOcclude->BindPipeline(m_TerrainAsync[m_ResourceIndex].Commands);
			m_UBOTempSets[m_ResourceIndex]->BindSet(0, m_TerrainAsync[m_ResourceIndex].Commands, *m_GrassOcclude);
			m_GrassSet[m_ResourceIndex]->BindSet(1, m_TerrainAsync[m_ResourceIndex].Commands, *m_GrassOcclude);
			vkCmdDispatchIndirect(m_TerrainAsync[m_ResourceIndex].Commands, m_GrassIndirectRef->GetBuffer(), 0);
		}

#if 0
		VkClearColorValue Color;
		Color.float32[0] = 0.0;
		vkCmdClearColorImage(m_TerrainAsync[m_ResourceIndex].Commands, m_WaterLUT[m_ResourceIndex].Image->GetImage(), VK_IMAGE_LAYOUT_GENERAL, &Color, 1, &m_WaterLUT[m_ResourceIndex].Image->GetSubResourceRange());

		m_WaterSet[m_ResourceIndex]->BindSet(0, m_TerrainAsync[m_ResourceIndex].Commands, *m_WaterCompute);
		m_WaterCompute->BindPipeline(m_TerrainAsync[m_ResourceIndex].Commands);
		
		for (int i = 0; i < 5; i++)
		{
			vkCmdDispatch(m_TerrainAsync[m_ResourceIndex].Commands, ceil(float(m_WaterLUT[0].Image->GetExtent().width) / 8.f), ceil(float(m_WaterLUT[0].Image->GetExtent().height) / 4.f), m_WaterLUT[0].Image->GetArrayLayers());
			m_WaterLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
		}
		m_WaterLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
#else
		// m_WaterLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
#endif

		m_TerrainLUT[m_ResourceIndex].Image->TransferOwnership(m_TerrainAsync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
		// m_WaterLUT[m_ResourceIndex].Image->TransferOwnership(m_TerrainAsync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());

		vkEndCommandBuffer(m_TerrainAsync[m_ResourceIndex].Commands);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_TerrainAsync[m_ResourceIndex].Commands;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_TerrainAsync[m_ResourceIndex].Semaphores[0];
		vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetQueue(), 1, &submitInfo, m_TerrainAsync[m_ResourceIndex].Fence);
	}

	// Begin IBL pass
	{
		vkWaitForFences(m_Scope.GetDevice(), 1, &m_CubemapAsync[m_ResourceIndex].Fence, VK_TRUE, UINT64_MAX);
		vkResetFences(m_Scope.GetDevice(), 1, &m_CubemapAsync[m_ResourceIndex].Fence);
		vkBeginCommandBuffer(m_CubemapAsync[m_ResourceIndex].Commands, &beginInfo);

		if (m_FrameCount >= m_ResourceCount)
		{
			m_SpecularLUT[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_CubemapAsync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
			m_DiffuseIrradience[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_CubemapAsync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
		}

		uint32_t mips = m_SpecularLUT[m_ResourceIndex].Image->GetMipLevelsCount();
		uint32_t X = CubeR / 8 + uint32_t(CubeR % 8 > 0);
		uint32_t Y = CubeR / 4 + uint32_t(CubeR % 4 > 0);

		m_CubemapLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
		m_SpecularLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
		m_DiffuseIrradience[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		m_CubemapPipeline->BindPipeline(m_CubemapAsync[m_ResourceIndex].Commands);
		m_CubemapDescriptors[m_ResourceIndex]->BindSet(0, m_CubemapAsync[m_ResourceIndex].Commands, *m_CubemapPipeline);
		vkCmdDispatch(m_CubemapAsync[m_ResourceIndex].Commands, X, Y, 6u);

		m_CubemapMipPipeline->BindPipeline(m_CubemapAsync[m_ResourceIndex].Commands);
		for (uint32_t mip = 1; mip < mips; mip++)
		{
			uint32_t ScaledR = CubeR >> mip;
			uint32_t scaledX = ScaledR / 8 + uint32_t(ScaledR % 8 > 0);
			uint32_t scaledY = ScaledR / 4 + uint32_t(ScaledR % 4 > 0);

			m_CubemapMipDescriptors[m_ResourceIndex * mips + mip]->BindSet(0, m_CubemapAsync[m_ResourceIndex].Commands, *m_CubemapMipPipeline);
			vkCmdDispatch(m_CubemapAsync[m_ResourceIndex].Commands, scaledX, scaledY, 6u);

			m_CubemapLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, 0, 6), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
		}
		m_CubemapLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		m_ConvolutionPipeline->BindPipeline(m_CubemapAsync[m_ResourceIndex].Commands);
		m_ConvolutionDescriptors[m_ResourceIndex]->BindSet(0, m_CubemapAsync[m_ResourceIndex].Commands, *m_ConvolutionPipeline);
		m_DiffuseDescriptors[m_ResourceIndex]->BindSet(1, m_CubemapAsync[m_ResourceIndex].Commands, *m_ConvolutionPipeline);
		vkCmdDispatch(m_CubemapAsync[m_ResourceIndex].Commands, X, Y, 6u);

		for (uint32_t mip = 1; mip < mips; mip++)
		{
			uint32_t ScaledR = CubeR >> mip;
			uint32_t scaledX = ceil(float(ScaledR) / 8.f);
			uint32_t scaledY = ceil(float(ScaledR) / 4.f);

			m_SpecularIBLPipeline->BindPipeline(m_CubemapAsync[m_ResourceIndex].Commands);
			m_ConvolutionDescriptors[m_ResourceIndex]->BindSet(0, m_CubemapAsync[m_ResourceIndex].Commands, *m_SpecularIBLPipeline);
			m_SpecularDescriptors[m_ResourceIndex * mips + mip]->BindSet(1, m_CubemapAsync[m_ResourceIndex].Commands, *m_SpecularIBLPipeline);

			float pushConstant = float(mip) / float(mips - 1.0);
			m_SpecularIBLPipeline->PushConstants(m_CubemapAsync[m_ResourceIndex].Commands, &pushConstant, sizeof(float), 0, VK_SHADER_STAGE_COMPUTE_BIT);
			vkCmdDispatch(m_CubemapAsync[m_ResourceIndex].Commands, scaledX, scaledY, 6u);
		}

		m_SpecularLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
		m_CubemapLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		VkImageCopy copy{};
		copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.dstSubresource.layerCount = 6;
		copy.dstSubresource.mipLevel = 0;
		copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.srcSubresource.layerCount = 6;
		copy.srcSubresource.mipLevel = 0;
		copy.extent = { CubeR, CubeR, 1 };
		vkCmdCopyImage(m_CubemapAsync[m_ResourceIndex].Commands, m_CubemapLUT[m_ResourceIndex].Image->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_SpecularLUT[m_ResourceIndex].Image->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

		m_CubemapLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
		m_SpecularLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
		m_SpecularLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_REMAINING_MIP_LEVELS, 0, 6), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		m_DiffuseIrradience[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		m_SpecularLUT[m_ResourceIndex].Image->TransferOwnership(m_CubemapAsync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
		m_DiffuseIrradience[m_ResourceIndex].Image->TransferOwnership(m_CubemapAsync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());

		vkEndCommandBuffer(m_CubemapAsync[m_ResourceIndex].Commands);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_CubemapAsync[m_ResourceIndex].Commands;
		submitInfo.signalSemaphoreCount = m_CubemapAsync[m_ResourceIndex].Semaphores.size();
		submitInfo.pSignalSemaphores = m_CubemapAsync[m_ResourceIndex].Semaphores.data();
		vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetQueue(), 1, &submitInfo, m_CubemapAsync[m_ResourceIndex].Fence);
	}

	// Draw Low Resolution Background
	{
		vkWaitForFences(m_Scope.GetDevice(), 1, &m_BackgroundAsync[m_ResourceIndex].Fence, VK_TRUE, UINT64_MAX);
		vkResetFences(m_Scope.GetDevice(), 1, &m_BackgroundAsync[m_ResourceIndex].Fence);
		vkBeginCommandBuffer(m_BackgroundAsync[m_ResourceIndex].Commands, &beginInfo);

		if (m_FrameCount > 1)
		{
			m_HdrAttachmentsLR[m_ResourceIndex]->TransferOwnership(m_BackgroundAsync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
			m_DepthAttachmentsLR[m_ResourceIndex]->TransferOwnership(m_BackgroundAsync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
		}
		m_HdrAttachmentsLR[m_ResourceIndex]->TransitionLayout(m_BackgroundAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
		// m_DepthAttachmentsLR[m_ResourceIndex]->TransitionLayout(m_BackgroundAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		double Re = glm::length(m_Camera.Transform.offset);
		ComputePipeline* Pipeline = Re < Rcb ? m_VolumetricsUnderPipeline.get() : (Re > Rct ? m_VolumetricsAbovePipeline.get() : m_VolumetricsBetweenPipeline.get());

		m_UBOTempSets[m_ResourceIndex]->BindSet(0, m_BackgroundAsync[m_ResourceIndex].Commands, *Pipeline);
		m_VolumetricsDescriptor->BindSet(1, m_BackgroundAsync[m_ResourceIndex].Commands, *Pipeline);
		m_TemporalVolumetrics[m_ResourceIndex]->BindSet(2, m_BackgroundAsync[m_ResourceIndex].Commands, *Pipeline);

		int Order = 0; // m_ResourceIndex
		Pipeline->PushConstants(m_BackgroundAsync[m_ResourceIndex].Commands, &Order, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		Pipeline->BindPipeline(m_BackgroundAsync[m_ResourceIndex].Commands);
 		vkCmdDispatch(m_BackgroundAsync[m_ResourceIndex].Commands, ceil(float(m_HdrAttachmentsLR[0]->GetExtent().width) / 16), ceil(float(m_HdrAttachmentsLR[0]->GetExtent().height) / 4), 1);

		m_HdrAttachmentsLR[m_ResourceIndex]->TransitionLayout(m_BackgroundAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
		m_DepthAttachmentsLR[m_ResourceIndex]->TransitionLayout(m_BackgroundAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		m_UBOTempSets[m_ResourceIndex]->BindSet(0, m_BackgroundAsync[m_ResourceIndex].Commands, *m_VolumetricsComposePipeline);
		m_TemporalVolumetrics[m_ResourceIndex]->BindSet(1, m_BackgroundAsync[m_ResourceIndex].Commands, *m_VolumetricsComposePipeline);
		m_VolumetricsComposePipeline->BindPipeline(m_BackgroundAsync[m_ResourceIndex].Commands);
		m_VolumetricsComposePipeline->PushConstants(m_BackgroundAsync[m_ResourceIndex].Commands, &Order, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		vkCmdDispatch(m_BackgroundAsync[m_ResourceIndex].Commands, ceil(float(m_HdrAttachmentsLR[0]->GetExtent().width) / 16), ceil(float(m_HdrAttachmentsLR[0]->GetExtent().height) / 4), 1);

		m_HdrAttachmentsLR[m_ResourceIndex]->TransitionLayout(m_BackgroundAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
		m_DepthAttachmentsLR[m_ResourceIndex]->TransitionLayout(m_BackgroundAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		m_HdrAttachmentsLR[m_ResourceIndex]->TransferOwnership(VK_NULL_HANDLE, m_BackgroundAsync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
		m_DepthAttachmentsLR[m_ResourceIndex]->TransferOwnership(VK_NULL_HANDLE, m_BackgroundAsync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());

		vkEndCommandBuffer(m_BackgroundAsync[m_ResourceIndex].Commands);

		m_BackgroundAsync[m_ResourceIndex].waitSemaphores = { };
		m_BackgroundAsync[m_ResourceIndex].waitStages = { };
		if (m_FrameCount > 0)
		{
			m_BackgroundAsync[m_ResourceIndex].waitSemaphores.push_back(m_ApplySync[WRAPL(m_ResourceIndex)].Semaphores[1]);
			m_BackgroundAsync[m_ResourceIndex].waitStages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_BackgroundAsync[m_ResourceIndex].Commands;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_BackgroundAsync[m_ResourceIndex].Semaphores[0];
		submitInfo.waitSemaphoreCount = m_BackgroundAsync[m_ResourceIndex].waitSemaphores.size();
		submitInfo.pWaitSemaphores = m_BackgroundAsync[m_ResourceIndex].waitSemaphores.data();
		submitInfo.pWaitDstStageMask = m_BackgroundAsync[m_ResourceIndex].waitStages.data();
		vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetQueue(), 1, &submitInfo, m_BackgroundAsync[m_ResourceIndex].Fence);
	}

	vkWaitForFences(m_Scope.GetDevice(), 1, &m_GraphicsFences[m_ResourceIndex], VK_TRUE, UINT64_MAX);
	vkResetFences(m_Scope.GetDevice(), 1, &m_GraphicsFences[m_ResourceIndex]);
	vkAcquireNextImageKHR(m_Scope.GetDevice(), m_Scope.GetSwapchain(), 0, m_SwapchainSemaphores[m_ResourceIndex], m_AcquireFence, &m_ImageIndex[m_ResourceIndex]);

	// Start deferred
	{
		vkBeginCommandBuffer(m_DeferredSync[m_ResourceIndex].Commands, &beginInfo);

		// Prepare targets
		{
			if (m_TerrainCompute.get())
			{
				m_TerrainLUT[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_DeferredSync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
				// m_WaterLUT[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_DeferredSync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
				m_TerrainLUT[WRAPR(m_ResourceIndex)].Image->TransferOwnership(m_DeferredSync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
				// m_WaterLUT[WRAPR(m_ResourceIndex)].Image->TransferOwnership(m_DeferredSync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
			}

			m_HdrAttachmentsHR[m_ResourceIndex]->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
			m_DepthHR[m_ResourceIndex].Image->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
			m_BlurAttachments[2 * m_ResourceIndex]->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_GRAPHICS_BIT);
			m_BlurAttachments[2 * m_ResourceIndex + 1]->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_GRAPHICS_BIT);
		}

		VkBufferCopy region{};
		region.size = sizeof(UniformBuffer);
		vkCmdCopyBuffer(m_DeferredSync[m_ResourceIndex].Commands, m_UBOTempBuffers[m_ResourceIndex]->GetBuffer(), m_UBOBuffers[m_ResourceIndex]->GetBuffer(), 1, &region);
		vkCmdCopyBuffer(m_DeferredSync[m_ResourceIndex].Commands, m_UBOTempBuffers[m_ResourceIndex]->GetBuffer(), m_UBOSkyBuffers[WRAPR(m_ResourceIndex)]->GetBuffer(), 1, &region);

		std::array<VkClearValue, 4> clearValues;
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[3].depthStencil = { 0.0f, 0 };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = m_FramebuffersHR[m_ResourceIndex];
		renderPassInfo.renderPass = m_Scope.GetRenderPass();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_Scope.GetSwapchainExtent();
		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(m_Scope.GetSwapchainExtent().width);
		viewport.height = static_cast<float>(m_Scope.GetSwapchainExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(m_DeferredSync[m_ResourceIndex].Commands, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Scope.GetSwapchainExtent();
		vkCmdSetScissor(m_DeferredSync[m_ResourceIndex].Commands, 0, 1, &scissor);

		vkCmdBeginRenderPass(m_DeferredSync[m_ResourceIndex].Commands, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

#ifdef INCLUDE_GUI
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
#endif

#if DEBUG == 1
		m_InFrame = true;
#endif

		return true;
	}
}

void VulkanBase::EndFrame()
{
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0)
		return;

	assert(m_InFrame);

	{
		vkCmdEndRenderPass(m_DeferredSync[m_ResourceIndex].Commands);
		vkEndCommandBuffer(m_DeferredSync[m_ResourceIndex].Commands);

		m_DeferredSync[m_ResourceIndex].waitStages = { };
		m_DeferredSync[m_ResourceIndex].waitSemaphores = { };

		if (m_TerrainCompute.get())
		{
			m_DeferredSync[m_ResourceIndex].waitSemaphores.push_back(m_TerrainAsync[m_ResourceIndex].Semaphores[0]);
			m_DeferredSync[m_ResourceIndex].waitStages.push_back(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = m_DeferredSync[m_ResourceIndex].waitSemaphores.size();
		submitInfo.pWaitSemaphores = m_DeferredSync[m_ResourceIndex].waitSemaphores.data();
		submitInfo.pWaitDstStageMask = m_DeferredSync[m_ResourceIndex].waitStages.data();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_DeferredSync[m_ResourceIndex].Commands;
		submitInfo.signalSemaphoreCount = m_DeferredSync[m_ResourceIndex].Semaphores.size();
		submitInfo.pSignalSemaphores = m_DeferredSync[m_ResourceIndex].Semaphores.data();
		m_GraphicsSubmits.push_back(submitInfo);
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// Draw High Resolution Objects
	{
		vkBeginCommandBuffer(m_ComposeSync[m_ResourceIndex].Commands, &beginInfo);

		m_SpecularLUT[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_ComposeSync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
		m_DiffuseIrradience[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_ComposeSync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());

		std::array<VkClearValue, 1> clearValues;
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = m_FramebuffersCP[m_ResourceIndex];
		renderPassInfo.renderPass = m_Scope.GetCompositionPass();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_Scope.GetSwapchainExtent();
		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(m_Scope.GetSwapchainExtent().width);
		viewport.height = static_cast<float>(m_Scope.GetSwapchainExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(m_ComposeSync[m_ResourceIndex].Commands, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Scope.GetSwapchainExtent();
		vkCmdSetScissor(m_ComposeSync[m_ResourceIndex].Commands, 0, 1, &scissor);

		vkCmdBeginRenderPass(m_ComposeSync[m_ResourceIndex].Commands, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_CompositionDescriptors[m_ResourceIndex]->BindSet(0, m_ComposeSync[m_ResourceIndex].Commands, *m_CompositionPipeline);
		m_CompositionPipeline->BindPipeline(m_ComposeSync[m_ResourceIndex].Commands);
		vkCmdDraw(m_ComposeSync[m_ResourceIndex].Commands, 3, 1, 0, 0);
		vkCmdEndRenderPass(m_ComposeSync[m_ResourceIndex].Commands);

		m_SpecularLUT[m_ResourceIndex].Image->TransferOwnership(m_ComposeSync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
		m_DiffuseIrradience[m_ResourceIndex].Image->TransferOwnership(m_ComposeSync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());

		vkEndCommandBuffer(m_ComposeSync[m_ResourceIndex].Commands);

		m_ComposeSync[m_ResourceIndex].waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
		m_ComposeSync[m_ResourceIndex].waitSemaphores = { m_DeferredSync[m_ResourceIndex].Semaphores[0], m_CubemapAsync[m_ResourceIndex].Semaphores[0] };

		// if we just started drawing, we should skip image present semaphore as it has not yet been signaled
		if (m_FrameCount >= m_ResourceCount)
		{
			m_ComposeSync[m_ResourceIndex].waitSemaphores.push_back(m_PresentSync[m_ResourceIndex].Semaphores[0]);
			m_ComposeSync[m_ResourceIndex].waitStages.push_back(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = m_ComposeSync[m_ResourceIndex].waitSemaphores.size();
		submitInfo.pWaitSemaphores = m_ComposeSync[m_ResourceIndex].waitSemaphores.data();
		submitInfo.pWaitDstStageMask = m_ComposeSync[m_ResourceIndex].waitStages.data();
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_ComposeSync[m_ResourceIndex].Semaphores[0];
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_ComposeSync[m_ResourceIndex].Commands;
		m_GraphicsSubmits.push_back(submitInfo);
	}

	{
		vkBeginCommandBuffer(m_ApplySync[m_ResourceIndex].Commands, &beginInfo);

		m_HdrAttachmentsLR[m_ResourceIndex]->TransferOwnership(VK_NULL_HANDLE, m_ApplySync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
		m_DepthAttachmentsLR[m_ResourceIndex]->TransferOwnership(VK_NULL_HANDLE, m_ApplySync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());

		uint32_t X = ceil(float(m_Scope.GetSwapchainExtent().width) / 8.f);
		uint32_t Y = ceil(float(m_Scope.GetSwapchainExtent().height) / 4.f);
		
		if (m_FrameCount + 1 >= m_ResourceCount)
			m_DepthAttachmentsLR[WRAPR(m_ResourceIndex)]->TransitionLayout(m_ApplySync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_GRAPHICS_BIT);

		m_BlendingPipeline->BindPipeline(m_ApplySync[m_ResourceIndex].Commands);
		m_BlendingDescriptors[m_ResourceIndex]->BindSet(0, m_ApplySync[m_ResourceIndex].Commands, *m_BlendingPipeline);
		vkCmdDispatch(m_ApplySync[m_ResourceIndex].Commands, X, Y, 1);

		m_HdrAttachmentsHR[m_ResourceIndex]->TransitionLayout(m_ApplySync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
		// m_DepthAttachmentsLR[WRAPR(m_ResourceIndex)]->TransitionLayout(m_ApplySync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);

		m_HdrAttachmentsLR[WRAPR(m_ResourceIndex)]->TransferOwnership(m_ApplySync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
		m_DepthAttachmentsLR[WRAPR(m_ResourceIndex)]->TransferOwnership(m_ApplySync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());

		vkEndCommandBuffer(m_ApplySync[m_ResourceIndex].Commands);

		m_ApplySync[m_ResourceIndex].waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
		m_ApplySync[m_ResourceIndex].waitSemaphores = { m_ComposeSync[m_ResourceIndex].Semaphores[0], m_BackgroundAsync[m_ResourceIndex].Semaphores[0] };

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = m_ApplySync[m_ResourceIndex].waitSemaphores.size();
		submitInfo.pWaitSemaphores = m_ApplySync[m_ResourceIndex].waitSemaphores.data();
		submitInfo.pWaitDstStageMask = m_ApplySync[m_ResourceIndex].waitStages.data();
		submitInfo.signalSemaphoreCount = m_ApplySync[m_ResourceIndex].Semaphores.size();
		submitInfo.pSignalSemaphores = m_ApplySync[m_ResourceIndex].Semaphores.data();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_ApplySync[m_ResourceIndex].Commands;
		m_GraphicsSubmits.push_back(submitInfo);
	}

	// Post processing and present
	{
		vkBeginCommandBuffer(m_PresentSync[m_ResourceIndex].Commands, &beginInfo);

		uint32_t X = ceil(float(m_Scope.GetSwapchainExtent().width) / 8.f);
		uint32_t Y = ceil(float(m_Scope.GetSwapchainExtent().height) / 4.f);

		std::array<VkImageMemoryBarrier, 2> barrier{};
		barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier[0].image = m_BlurAttachments[2 * m_ResourceIndex]->GetImage();
		barrier[0].subresourceRange = m_BlurAttachments[2 * m_ResourceIndex]->GetSubResourceRange();
		barrier[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		barrier[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier[1].image = m_BlurAttachments[2 * m_ResourceIndex + 1]->GetImage();
		barrier[1].subresourceRange = m_BlurAttachments[2 * m_ResourceIndex + 1]->GetSubResourceRange();
		barrier[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		const int Radius = 13;
		m_BlurSetupPipeline->BindPipeline(m_PresentSync[m_ResourceIndex].Commands);
		m_BlurDescriptors[m_ResourceIndex]->BindSet(0, m_PresentSync[m_ResourceIndex].Commands, *m_BlurSetupPipeline);
		vkCmdDispatch(m_PresentSync[m_ResourceIndex].Commands, X, Y, 1);

		for (int i = 0; i <= Radius; i++)
		{
			if (i % 2 == 0)
			{
				m_BlurHorizontalPipeline->BindPipeline(m_PresentSync[m_ResourceIndex].Commands);
				m_BlurDescriptors[m_ResourceIndex]->BindSet(0, m_PresentSync[m_ResourceIndex].Commands, *m_BlurHorizontalPipeline);
			}
			else
			{
				m_BlurVerticalPipeline->BindPipeline(m_PresentSync[m_ResourceIndex].Commands);
				m_BlurDescriptors[m_ResourceIndex]->BindSet(0, m_PresentSync[m_ResourceIndex].Commands, *m_BlurVerticalPipeline);
			}
			vkCmdDispatch(m_PresentSync[m_ResourceIndex].Commands, X, Y, 1);

			if (i == Radius)
			{
				barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				vkCmdPipelineBarrier(m_PresentSync[m_ResourceIndex].Commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, barrier.size(), barrier.data());
			}
			else if (i % 2 == 0)
			{
				vkCmdPipelineBarrier(m_PresentSync[m_ResourceIndex].Commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier[0]);
			}
			else
			{
				vkCmdPipelineBarrier(m_PresentSync[m_ResourceIndex].Commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier[1]);
			}
		}

		vkWaitForFences(m_Scope.GetDevice(), 1, &m_AcquireFence, VK_TRUE, UINT64_MAX);
		vkResetFences(m_Scope.GetDevice(), 1, &m_AcquireFence);

		std::array<VkClearValue, 1> clearValues;
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = m_FramebuffersPP[m_ImageIndex[m_ResourceIndex]];
		renderPassInfo.renderPass = m_Scope.GetPostProcessPass();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_Scope.GetSwapchainExtent();
		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(m_Scope.GetSwapchainExtent().width);
		viewport.height = static_cast<float>(m_Scope.GetSwapchainExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(m_PresentSync[m_ResourceIndex].Commands, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Scope.GetSwapchainExtent();
		vkCmdSetScissor(m_PresentSync[m_ResourceIndex].Commands, 0, 1, &scissor);

		vkCmdBeginRenderPass(m_PresentSync[m_ResourceIndex].Commands, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_UBOSets[m_ResourceIndex]->BindSet(0, m_PresentSync[m_ResourceIndex].Commands, *m_PostProcessPipeline);
		m_PostProcessDescriptors[m_ResourceIndex]->BindSet(1, m_PresentSync[m_ResourceIndex].Commands, *m_PostProcessPipeline);
		m_PostProcessPipeline->BindPipeline(m_PresentSync[m_ResourceIndex].Commands);
		vkCmdDraw(m_PresentSync[m_ResourceIndex].Commands, 3, 1, 0, 0);

		vkCmdNextSubpass(m_PresentSync[m_ResourceIndex].Commands, VK_SUBPASS_CONTENTS_INLINE);

#ifdef INCLUDE_GUI
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_PresentSync[m_ResourceIndex].Commands);
#endif

		vkCmdEndRenderPass(m_PresentSync[m_ResourceIndex].Commands);
	}

	vkEndCommandBuffer(m_PresentSync[m_ResourceIndex].Commands);

	// Submit final image
	{
		m_PresentSync[m_ResourceIndex].waitSemaphores = { m_SwapchainSemaphores[m_ResourceIndex], m_ApplySync[m_ResourceIndex].Semaphores[0] };
		m_PresentSync[m_ResourceIndex].waitStages = { VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = m_PresentSync[m_ResourceIndex].waitSemaphores.size();
		submitInfo.pWaitSemaphores = m_PresentSync[m_ResourceIndex].waitSemaphores.data();
		submitInfo.pWaitDstStageMask = m_PresentSync[m_ResourceIndex].waitStages.data();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_PresentSync[m_ResourceIndex].Commands;
		submitInfo.signalSemaphoreCount = m_PresentSync[m_ResourceIndex].Semaphores.size();
		submitInfo.pSignalSemaphores = m_PresentSync[m_ResourceIndex].Semaphores.data();
		m_GraphicsSubmits.push_back(submitInfo);
	}

	// Submit queues
	{
		VkResult res = vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), m_GraphicsSubmits.size(), m_GraphicsSubmits.data(), m_GraphicsFences[m_ResourceIndex]);
		assert(res != VK_ERROR_DEVICE_LOST);
	}

	// Present final image
	{
		std::vector<VkSemaphore> waitSemaphores = { m_PresentSync[m_ResourceIndex].Semaphores[1] };
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = waitSemaphores.size();
		presentInfo.pWaitSemaphores = waitSemaphores.data();
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_Scope.GetSwapchain();
		presentInfo.pImageIndices = &m_ImageIndex[m_ResourceIndex];
		presentInfo.pResults = VK_NULL_HANDLE;

		vkQueuePresentKHR(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), &presentInfo);
	}

	m_ResourceIndex = (m_ResourceIndex + 1) % m_ResourceCount;
	m_FrameCount = m_FrameCount + 1 == UINT64_MAX ? m_ResourceCount + 1 : m_FrameCount + 1;

#if DEBUG == 1
	m_InFrame = false;
#endif
}

void VulkanBase::_beginTerrainPass() const
{
	vkCmdEndRenderPass(m_DeferredSync[m_ResourceIndex].Commands);

	m_DepthHR[m_ResourceIndex].Image->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
	m_DepthHR[m_ResourceIndex].Image->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, 0, 1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);

	VkImageBlit blit{};
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	blit.srcSubresource.layerCount = 1;
	blit.srcSubresource.mipLevel = 0;
	blit.dstSubresource.layerCount = 1;
	blit.dstSubresource.mipLevel = 1;
	blit.srcOffsets[1].x = m_DepthHR[m_ResourceIndex].Image->GetExtent().width;
	blit.srcOffsets[1].y = m_DepthHR[m_ResourceIndex].Image->GetExtent().height;
	blit.srcOffsets[1].z = 1;
	blit.dstOffsets[1].x = m_DepthHR[m_ResourceIndex].Image->GetExtent().width / 2;
	blit.dstOffsets[1].y = m_DepthHR[m_ResourceIndex].Image->GetExtent().height / 2;
	blit.dstOffsets[1].z = 1;
	vkCmdBlitImage(m_DeferredSync[m_ResourceIndex].Commands, m_DepthHR[m_ResourceIndex].Image->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_DepthHR[m_ResourceIndex].Image->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

	m_DepthHR[m_ResourceIndex].Image->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
	m_DepthHR[m_ResourceIndex].Image->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, 0, 1), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);

	std::array<VkClearValue, 4> clearValues;
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[3].depthStencil = { 0.0f, 0 };

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.framebuffer = m_FramebuffersTR[m_ResourceIndex];
	renderPassInfo.renderPass = m_Scope.GetTerrainPass();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = m_Scope.GetSwapchainExtent();
	renderPassInfo.clearValueCount = clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();

	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = static_cast<float>(m_Scope.GetSwapchainExtent().width);
	viewport.height = static_cast<float>(m_Scope.GetSwapchainExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(m_DeferredSync[m_ResourceIndex].Commands, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_Scope.GetSwapchainExtent();
	vkCmdSetScissor(m_DeferredSync[m_ResourceIndex].Commands, 0, 1, &scissor);

	vkCmdBeginRenderPass(m_DeferredSync[m_ResourceIndex].Commands, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanBase::_handleResize()
{
	Wait();

	std::erase_if(m_FramebuffersHR, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersCP, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersTR, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersPP, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_SwapchainViews, [&, this](VkImageView& view) {
		vkDestroyImageView(m_Scope.GetDevice(), view, VK_NULL_HANDLE);
		return true;
	});

	m_HdrAttachmentsHR.resize(0);
	m_HdrViewsHR.resize(0);

	m_DeferredAttachments.resize(0);
	m_DeferredViews.resize(0);

	m_BlurAttachments.resize(0);
	m_BlurViews.resize(0);

	m_NormalAttachments.resize(0);
	m_NormalViews.resize(0);

	m_DepthHR.resize(0);

	m_HdrAttachmentsLR.resize(0);
	m_HdrViewsLR.resize(0);

	m_DepthAttachmentsLR.resize(0);
	m_DepthViewsLR.resize(0);

	m_TemporalVolumetrics.resize(0);

	m_SwapchainImages.resize(0);

	m_Scope.RecreateSwapchain(m_Surface);

	if (m_Scope.GetSwapchain() == VK_NULL_HANDLE)
		return;

	create_swapchain_images();
	create_framebuffers();
	create_frame_descriptors();

	m_Camera.Projection.SetAspect(static_cast<float>(m_Scope.GetSwapchainExtent().width) / static_cast<float>(m_Scope.GetSwapchainExtent().height));

	//it shouldn't change afaik, but better to keep it under control
	assert(m_SwapchainImages.size() == m_PresentSync.size());
}

void VulkanBase::Wait() const
{
	vkDeviceWaitIdle(m_Scope.GetDevice());
}

void VulkanBase::SetCloudLayerSettings(CloudLayerProfile settings)
{
	cloudParams.Coverage = settings.Coverage;
	cloudParams.CoverageSq = settings.Coverage * settings.Coverage;
	cloudParams.HeightFactor = glm::pow(settings.Coverage, 1.125);
	cloudParams.BottomSmoothnessFactor = glm::mix(0.05, 0.5, glm::smoothstep(0.0f, 1.0f, cloudParams.CoverageSq));
	cloudParams.LightIntensity = glm::mix(0.05, 1.0, glm::max(0.7 - cloudParams.CoverageSq, 0.0) / 0.7);
	cloudParams.Ambient = glm::mix(0.0, 0.01, (cloudParams.LightIntensity - 0.05) / 0.95);
	cloudParams.Density = settings.Density;
	cloudParams.TopBound = Rct;
	cloudParams.BottomBound = Rcb + (Rct - Rcb) * (0.5 - glm::min(cloudParams.Coverage, 0.5f));
	cloudParams.BoundDelta = cloudParams.TopBound - cloudParams.BottomBound;

	m_CloudLayer->Update(&cloudParams, sizeof(CloudParameters));
}

void VulkanBase::SetTerrainLayerSettings(float Scale, int Count, TerrainLayerProfile* settings)
{
	assert(Count < 100, "Too many layers!");

	m_TerrainLayer->Update(&Count, sizeof(int));
	m_TerrainLayer->Update(&Scale, sizeof(float), sizeof(int));
	m_TerrainLayer->Update(settings, Count * sizeof(TerrainLayerProfile), 16u);
}

#pragma region Initialization
std::vector<const char*> VulkanBase::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> rqextensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef VALIDATION
	rqextensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	return rqextensions;
}

VkBool32 VulkanBase::create_instance()
{
#ifdef VALIDATION
	assert(checkValidationLayerSupport());
#endif

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "AVR_App";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
	appInfo.pEngineName = "AVR";
	appInfo.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	const auto& ext = getRequiredExtensions();

	createInfo.enabledExtensionCount = ext.size();
	createInfo.ppEnabledExtensionNames = ext.data();

#ifdef VALIDATION
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
	createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
	populateDebugMessengerCreateInfo(debugCreateInfo);
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = VK_NULL_HANDLE;
#endif
	return vkCreateInstance(&createInfo, VK_NULL_HANDLE, &m_VkInstance) == VK_SUCCESS;
}

VkBool32 VulkanBase::create_swapchain_images()
{
	assert(m_Scope.GetSwapchain() != VK_NULL_HANDLE);

	uint32_t imagesCount = m_Scope.GetMaxFramesInFlight();
	m_ResourceCount = glm::max(imagesCount, 2u);

	m_SwapchainImages.resize(imagesCount);
	m_SwapchainViews.resize(imagesCount);

	m_DepthHR.resize(m_ResourceCount);

	m_HdrAttachmentsHR.resize(m_ResourceCount);
	m_HdrViewsHR.resize(m_ResourceCount);

	m_DeferredAttachments.resize(m_ResourceCount);
	m_DeferredViews.resize(m_ResourceCount);

	m_BlurAttachments.resize(2 * m_ResourceCount);
	m_BlurViews.resize(2 * m_ResourceCount);

	m_NormalAttachments.resize(m_ResourceCount);
	m_NormalViews.resize(m_ResourceCount);

	m_HdrAttachmentsLR.resize(m_ResourceCount);
	m_HdrViewsLR.resize(m_ResourceCount);

	m_DepthAttachmentsLR.resize(m_ResourceCount);
	m_DepthViewsLR.resize(m_ResourceCount);

	VkBool32 res = vkGetSwapchainImagesKHR(m_Scope.GetDevice(), m_Scope.GetSwapchain(), &imagesCount, m_SwapchainImages.data()) == VK_SUCCESS;

	std::vector<uint32_t> queueFamilies = { m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex() };
	std::sort(queueFamilies.begin(), queueFamilies.end());
	queueFamilies.resize(std::distance(queueFamilies.begin(), std::unique(queueFamilies.begin(), queueFamilies.end())));

	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	for (size_t i = 0; i < m_ResourceCount; i++)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_SwapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.format = m_Scope.GetColorFormat();

		VkImageCreateInfo hdrInfo{};
		hdrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		hdrInfo.format = m_Scope.GetHDRFormat();
		hdrInfo.arrayLayers = 1;
		hdrInfo.extent = { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 };
		hdrInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		hdrInfo.imageType = VK_IMAGE_TYPE_2D;
		hdrInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		hdrInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		hdrInfo.mipLevels = 1;
		hdrInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		hdrInfo.queueFamilyIndexCount = queueFamilies.size();
		hdrInfo.pQueueFamilyIndices = queueFamilies.data();

		VkImageCreateInfo depthInfo{};
		depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		depthInfo.format = m_Scope.GetDepthFormat();
		depthInfo.arrayLayers = 1;
		depthInfo.extent = { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 };
		depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		depthInfo.imageType = VK_IMAGE_TYPE_2D;
		depthInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		depthInfo.mipLevels = 1;
		depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		depthInfo.queueFamilyIndexCount = queueFamilies.size();
		depthInfo.pQueueFamilyIndices = queueFamilies.data();

		res = (vkCreateImageView(m_Scope.GetDevice(), &viewInfo, VK_NULL_HANDLE, &m_SwapchainViews[i]) == VK_SUCCESS) & res;

		hdrInfo.usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		m_HdrAttachmentsHR[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_HdrViewsHR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_HdrAttachmentsHR[i]);

		hdrInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		m_BlurAttachments[2 * i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_BlurViews[2 * i] = std::make_unique<VulkanImageView>(m_Scope, *m_BlurAttachments[2 * i]);

		m_BlurAttachments[2 * i + 1] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_BlurViews[2 * i + 1] = std::make_unique<VulkanImageView>(m_Scope, *m_BlurAttachments[2 * i + 1]);

		hdrInfo.usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		hdrInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		m_DeferredAttachments[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_DeferredViews[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DeferredAttachments[i]);

		hdrInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		m_NormalAttachments[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_NormalViews[i] = std::make_unique<VulkanImageView>(m_Scope, *m_NormalAttachments[i]);

		depthInfo.usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		depthInfo.mipLevels = 2;
		m_DepthHR[i].Image = std::make_unique<VulkanImage>(m_Scope, depthInfo, allocCreateInfo);

		VkImageSubresourceRange depthRange = m_DepthHR[i].Image->GetSubResourceRange();

		depthRange.levelCount = 1;
		depthRange.baseMipLevel = 0;
		m_DepthHR[i].Views.push_back(std::make_unique<VulkanImageView>(m_Scope, *m_DepthHR[i].Image, depthRange));
		depthRange.baseMipLevel = 1;
		m_DepthHR[i].Views.push_back(std::make_unique<VulkanImageView>(m_Scope, *m_DepthHR[i].Image, depthRange));

		hdrInfo.extent.width /= LRr;
		hdrInfo.extent.height /= LRr;
		hdrInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		hdrInfo.format = m_Scope.GetHDRFormat();
		m_HdrAttachmentsLR[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_HdrViewsLR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_HdrAttachmentsLR[i]);

		depthInfo.extent = { m_Scope.GetSwapchainExtent().width / LRr, m_Scope.GetSwapchainExtent().height / LRr, 1 };
		depthInfo.format = VK_FORMAT_R32G32_SFLOAT;
		depthInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		depthInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
		depthInfo.mipLevels = 1;
		m_DepthAttachmentsLR[i] = std::make_unique<VulkanImage>(m_Scope, depthInfo, allocCreateInfo);
		m_DepthViewsLR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DepthAttachmentsLR[i]);
	}

	return res;
}

VkBool32 VulkanBase::create_framebuffers()
{
	assert(m_ResourceCount > 0 && m_Scope.GetRenderPass() != VK_NULL_HANDLE);

	m_FramebuffersHR.resize(m_ResourceCount);
	m_FramebuffersTR.resize(m_ResourceCount);
	m_FramebuffersCP.resize(m_ResourceCount);
	m_FramebuffersPP.resize(m_ResourceCount);

	VkBool32 res = 1;
	for (size_t i = 0; i < m_ResourceCount; i++)
	{
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetRenderPass(), { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 }, { m_HdrViewsHR[i]->GetImageView(), m_NormalViews[i]->GetImageView(),m_DeferredViews[i]->GetImageView(), m_DepthHR[i].Views[0]->GetImageView()}, &m_FramebuffersHR[i]) & res;
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetTerrainPass(), { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 }, { m_HdrViewsHR[i]->GetImageView(), m_NormalViews[i]->GetImageView(),m_DeferredViews[i]->GetImageView(), m_DepthHR[i].Views[0]->GetImageView() }, &m_FramebuffersTR[i]) & res;
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetCompositionPass(), { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 }, { m_HdrViewsHR[i]->GetImageView() }, &m_FramebuffersCP[i]);
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetPostProcessPass(), { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 }, { m_SwapchainViews[i] }, &m_FramebuffersPP[i]);
	}

	return res;
}

VkBool32 VulkanBase::create_frame_pipelines()
{
	VkBool32 res = 1;

	m_CompositionPipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("composition_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(m_CompositionDescriptors[0]->GetLayout())
		.AddSpecializationConstant(0, Rg, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddSpecializationConstant(1, Rt, VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetRenderPass(m_Scope.GetCompositionPass(), 0)
		.Construct(m_Scope);

	m_PostProcessPipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("post_process_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(m_PostProcessDescriptors[0]->GetLayout())
		.SetRenderPass(m_Scope.GetPostProcessPass(), 0)
		.Construct(m_Scope);

	m_BlurSetupPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_BlurDescriptors[0]->GetLayout())
		.SetShaderName("blur_setup_comp")
		.Construct(m_Scope);

	m_BlurHorizontalPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_BlurDescriptors[0]->GetLayout())
		.SetShaderName("blur_horizontal_comp")
		.Construct(m_Scope);

	m_BlurVerticalPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_BlurDescriptors[0]->GetLayout())
		.SetShaderName("blur_vertical_comp")
		.Construct(m_Scope);

	m_BlendingPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_BlendingDescriptors[0]->GetLayout())
		.SetShaderName("scene_blend_comp")
		.Construct(m_Scope);

	return res;
}

VkBool32 VulkanBase::prepare_renderer_resources()
{
	VkBool32 res = 1;

	VkBufferCreateInfo uboInfo{};
	uboInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	uboInfo.size = sizeof(UniformBuffer);
	uboInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo uboAllocCreateInfo1{};
	uboAllocCreateInfo1.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VmaAllocationCreateInfo uboAllocCreateInfo2{};
	uboAllocCreateInfo2.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	m_UBOTempBuffers.resize(m_ResourceCount);
	m_UBOSkyBuffers.resize(m_ResourceCount);
	m_UBOBuffers.resize(m_ResourceCount);

	m_UBOTempSets.resize(m_ResourceCount);
	m_UBOSkySets.resize(m_ResourceCount);
	m_UBOSets.resize(m_ResourceCount);

	for (uint32_t i = 0; i < m_ResourceCount; i++)
	{
		uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		m_UBOBuffers[i] = std::make_unique<Buffer>(m_Scope, uboInfo, uboAllocCreateInfo1);
		m_UBOSkyBuffers[i] = std::make_unique<Buffer>(m_Scope, uboInfo, uboAllocCreateInfo1);

		uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		m_UBOTempBuffers[i] = std::make_unique<Buffer>(m_Scope, uboInfo, uboAllocCreateInfo2);

		m_UBOSets[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_ALL, *m_UBOBuffers[i])
			.Allocate(m_Scope);

		m_UBOSkySets[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_ALL, *m_UBOSkyBuffers[i])
			.Allocate(m_Scope);

		m_UBOTempSets[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_ALL, *m_UBOTempBuffers[i])
			.Allocate(m_Scope);
	}

	m_DefaultWhite  = std::make_shared<VulkanTextureMultiView>();
	m_DefaultBlack  = std::make_shared<VulkanTextureMultiView>();
	m_DefaultNormal = std::make_shared<VulkanTextureMultiView>();
	m_DefaultARM    = std::make_shared<VulkanTextureMultiView>();

	m_DefaultWhite->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(255u), std::byte(255u), std::byte(255u), std::byte(255u));
	m_DefaultBlack->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(0u));
	m_DefaultNormal->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(127u), std::byte(127u), std::byte(255u), std::byte(255u));
	m_DefaultARM->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(255u), std::byte(255u), std::byte(0u), std::byte(255u));

	VkImageSubresourceRange SubRange = m_DefaultWhite->Image->GetSubResourceRange();
	m_DefaultWhite->Views.push_back(std::make_unique<VulkanImageView>(m_Scope, *m_DefaultWhite->Image, SubRange, VK_IMAGE_VIEW_TYPE_2D));
	m_DefaultWhite->Views.push_back(std::make_unique<VulkanImageView>(m_Scope, *m_DefaultWhite->Image, SubRange, VK_IMAGE_VIEW_TYPE_2D_ARRAY));

	m_DefaultBlack->Views.push_back(std::make_unique<VulkanImageView>(m_Scope, *m_DefaultBlack->Image, SubRange, VK_IMAGE_VIEW_TYPE_2D));
	m_DefaultBlack->Views.push_back(std::make_unique<VulkanImageView>(m_Scope, *m_DefaultBlack->Image, SubRange, VK_IMAGE_VIEW_TYPE_2D_ARRAY));

	m_DefaultNormal->Views.push_back(std::make_unique<VulkanImageView>(m_Scope, *m_DefaultNormal->Image, SubRange, VK_IMAGE_VIEW_TYPE_2D));
	m_DefaultNormal->Views.push_back(std::make_unique<VulkanImageView>(m_Scope, *m_DefaultNormal->Image, SubRange, VK_IMAGE_VIEW_TYPE_2D_ARRAY));

	m_DefaultARM->Views.push_back(std::make_unique<VulkanImageView>(m_Scope, *m_DefaultARM->Image, SubRange, VK_IMAGE_VIEW_TYPE_2D));
	m_DefaultARM->Views.push_back(std::make_unique<VulkanImageView>(m_Scope, *m_DefaultARM->Image, SubRange, VK_IMAGE_VIEW_TYPE_2D_ARRAY));

	return res;
}

VkBool32 VulkanBase::create_frame_descriptors()
{
	VkBool32 res = 1;

	VkSampler SamplerPoint = m_Scope.GetSampler(ESamplerType::PointClamp, 1);
	VkSampler SamplerPointMirror = m_Scope.GetSampler(ESamplerType::PointMirror, 1);
	VkSampler SamplerLinear = m_Scope.GetSampler(ESamplerType::LinearClamp, 1);
	VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::LinearRepeat, 1);

	m_BlendingDescriptors.resize(m_ResourceCount);
	m_CompositionDescriptors.resize(m_ResourceCount);
	m_PostProcessDescriptors.resize(m_ResourceCount);
	m_TemporalVolumetrics.resize(m_ResourceCount);
	m_BlurDescriptors.resize(m_ResourceCount);
	m_SubpassDescriptors.resize(m_ResourceCount);

	for (uint32_t i = 0; i < m_ResourceCount; i++)
	{
		m_CompositionDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsHR[i]->GetImageView(), SamplerPoint, VK_IMAGE_LAYOUT_GENERAL)
			.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, m_NormalViews[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, m_DeferredViews[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(4, VK_SHADER_STAGE_FRAGMENT_BIT, m_DepthHR[i].Views[0]->GetImageView(), SamplerPoint)
			.AddImageSampler(5, VK_SHADER_STAGE_FRAGMENT_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(6, VK_SHADER_STAGE_FRAGMENT_BIT, m_IrradianceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(7, VK_SHADER_STAGE_FRAGMENT_BIT, m_ScatteringLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(8, VK_SHADER_STAGE_FRAGMENT_BIT, m_DiffuseIrradience[i].View->GetImageView(), SamplerLinear)
			.AddImageSampler(9, VK_SHADER_STAGE_FRAGMENT_BIT, m_SpecularLUT[i].Views[0]->GetImageView(), m_Scope.GetSampler(ESamplerType::LinearClamp, m_SpecularLUT[i].Views[0]->GetSubresourceRange().levelCount))
			.AddImageSampler(10, VK_SHADER_STAGE_FRAGMENT_BIT, m_BRDFLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(11, VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeShape.View->GetImageView(), m_Scope.GetSampler(ESamplerType::LinearClamp, m_VolumeShape.View->GetSubresourceRange().levelCount))
			.AddImageSampler(12, VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeWeather.View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, m_VolumeWeather.View->GetSubresourceRange().levelCount))
			.AddImageSampler(13, VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeWeatherCube.View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, m_VolumeWeatherCube.View->GetSubresourceRange().levelCount))
			.AddUniformBuffer(14, VK_SHADER_STAGE_FRAGMENT_BIT, *m_CloudLayer)
			.Allocate(m_Scope);

		m_PostProcessDescriptors[i] = DescriptorSetDescriptor()
			.AddImageSampler(0, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsHR[i]->GetImageView(), SamplerLinear)
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_BlurViews[2 * i + 1]->GetImageView(), SamplerLinear)
			.Allocate(m_Scope);

		m_BlurDescriptors[i] = DescriptorSetDescriptor()
			.AddImageSampler(0, VK_SHADER_STAGE_COMPUTE_BIT, m_HdrViewsHR[i]->GetImageView(), SamplerPoint)
			.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_BlurViews[2 * i]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL)
			.AddStorageImage(2, VK_SHADER_STAGE_COMPUTE_BIT, m_BlurViews[2 * i + 1]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL)
			.Allocate(m_Scope);

		m_TemporalVolumetrics[i] = DescriptorSetDescriptor()
			.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_HdrViewsLR[i]->GetImageView())
			.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_DepthViewsLR[i]->GetImageView())
			.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_HdrViewsLR[WRAPL(i)]->GetImageView(), SamplerLinear)
			.AddUniformBuffer(3, VK_SHADER_STAGE_COMPUTE_BIT, *m_UBOSkyBuffers[i])
			.Allocate(m_Scope);

		m_BlendingDescriptors[i] = DescriptorSetDescriptor()
			.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_HdrViewsHR[i]->GetImageView())
			.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_DepthViewsLR[WRAPR(i)]->GetImageView())
			.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_HdrViewsLR[i]->GetImageView(), SamplerLinear)
			.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, m_DepthViewsLR[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, m_DepthHR[i].Views[0]->GetImageView(), SamplerPoint)
			.Allocate(m_Scope);

		m_SubpassDescriptors[i] = DescriptorSetDescriptor()
			.AddSubpassAttachment(0, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsHR[i]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL)
			.AddSubpassAttachment(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_NormalViews[i]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL)
			.AddSubpassAttachment(2, VK_SHADER_STAGE_FRAGMENT_BIT, m_DeferredViews[i]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL)
			.AddSubpassAttachment(3, VK_SHADER_STAGE_FRAGMENT_BIT, m_DepthHR[i].Views[0]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL)
			.Allocate(m_Scope);

		if (m_GrassDrawSet.size() > 0)
		{
			m_GrassDrawSet[i] = DescriptorSetDescriptor()
				.AddImageSampler(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TerrainLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearClamp, 1))
				.AddStorageBuffer(1, VK_SHADER_STAGE_VERTEX_BIT, *TerrainVBs[i])
				.AddStorageBuffer(2, VK_SHADER_STAGE_VERTEX_BIT, *m_GrassPositions[i])
				.AddImageSampler(3, VK_SHADER_STAGE_VERTEX_BIT, m_DepthHR[i].Views[1]->GetImageView(), m_Scope.GetSampler(ESamplerType::PointClamp, 1))
				.Allocate(m_Scope);
		}
	}

	return res;
}
#pragma endregion

#pragma region Objects
std::unique_ptr<VulkanTexture> VulkanBase::_loadImage(const std::vector<std::string>& path, VkFormat format) const
{
	std::unique_ptr<VulkanTexture> Texture = std::make_unique<VulkanTexture>();

	unsigned char* all = nullptr;

	int w, h, c;
	unsigned char* pixels = stbi_load(path[0].c_str(), &w, &h, &c, 4);

	if (path.size() > 1)
	{
		all = (unsigned char*)malloc(w * h * 4 * path.size());
		memmove(all, pixels, w * h * 4);
		free(pixels);
	}
	else
	{
		all = pixels;
	}

	for (int i = 1; i < path.size(); i++)
	{
		int nw, nh, nc;
		pixels = stbi_load(path[i].c_str(), &nw, &nh, &nc, 4);

		assert(nw == w && nh == h);

		if (nw != w || nh != h)
		{
			free(pixels);
			free(all);

			Texture->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(255u), std::byte(255u), std::byte(255u), std::byte(255u));
			Texture->View = std::make_unique<VulkanImageView>(m_Scope, *Texture->Image);
			
			return Texture;
		}

		memmove(all + i * w * h * 4, pixels, w * h * 4);
		free(pixels);
	}

	Texture->Image = create_image(m_Scope, all, path.size(), w, h, 4, format, 0);
	Texture->View = std::make_unique<VulkanImageView>(m_Scope, *Texture->Image, Texture->Image->GetSubResourceRange());
	free(all);

	return Texture;
}

entt::entity VulkanBase::_constructShape(entt::entity ent, entt::registry& registry, const GR::Shapes::GeoClipmap& shape)
{
	PBRObject& gro = registry.emplace_or_replace<PBRObject>(ent);
	gro.mesh = shape.Generate(m_Scope, nullptr);

	const_cast<VulkanBase*>(this)->terrain_init(*gro.mesh->GetVertexBuffer(), shape);

	gro.descriptorSet = create_terrain_set(*m_DefaultWhite->Views[1], *m_DefaultNormal->Views[1], *m_DefaultARM->Views[1]);
	gro.pipeline = create_terrain_pipeline(*gro.descriptorSet, shape);

	registry.emplace_or_replace<GR::Components::AlbedoMap>(ent, m_DefaultWhite, &gro.dirty);
	registry.emplace_or_replace<GR::Components::NormalDisplacementMap>(ent, m_DefaultNormal, &gro.dirty);
	registry.emplace_or_replace<GR::Components::AORoughnessMetallicMapTransmittance>(ent, m_DefaultWhite, &gro.dirty);

	return ent;
}

entt::entity VulkanBase::_constructShape(entt::entity ent, entt::registry& registry, const GR::Shapes::Shape& shape, GR::Shapes::GeometryDescriptor* geometry)
{
	PBRObject& gro = registry.emplace_or_replace<PBRObject>(ent);
	gro.descriptorSet = create_pbr_set(*m_DefaultWhite->Views[0], *m_DefaultNormal->Views[0], *m_DefaultARM->Views[0]);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);
	gro.mesh = shape.Generate(m_Scope, geometry);

	registry.emplace_or_replace<GR::Components::AlbedoMap>(ent, m_DefaultWhite, &gro.dirty);
	registry.emplace_or_replace<GR::Components::NormalDisplacementMap>(ent, m_DefaultNormal, &gro.dirty);
	registry.emplace_or_replace<GR::Components::AORoughnessMetallicMapTransmittance>(ent, m_DefaultWhite, &gro.dirty);

	return ent;
}


#pragma endregion

#pragma region Layers

#ifdef VALIDATION

VkResult VulkanBase::createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != VK_NULL_HANDLE) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanBase::destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != VK_NULL_HANDLE) {
		func(instance, debugMessenger, pAllocator);
	}
}

void VulkanBase::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}

void VulkanBase::setupDebugMessenger()
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (createDebugUtilsMessengerEXT(m_VkInstance, &createInfo, VK_NULL_HANDLE, &m_DebugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

VkBool32 VulkanBase::checkValidationLayerSupport() const
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, VK_NULL_HANDLE);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : m_ValidationLayers)
	{
		VkBool32 layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

VkBool32 VulkanBase::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}

#endif
#pragma endregion