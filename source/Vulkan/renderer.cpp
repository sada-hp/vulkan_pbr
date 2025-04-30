#define VK_KHR_swapchain 
#define VMA_IMPLEMENTATION
#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION

#include "pch.hpp"
#include "renderer.hpp"
#include <stb/stb_image.h>

#ifdef INCLUDE_GUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"
#endif

#define WRAPL(i) i == 0 ? m_ResourceCount - 1 : i - 1
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

	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.format = imageCI.format;
	imageViewCI.viewType = (flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0 ? VK_IMAGE_VIEW_TYPE_CUBE : (subRes.layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
	imageViewCI.subresourceRange = subRes;

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
	VkPhysicalDeviceShaderAtomicFloatFeaturesEXT featureFloats{};
	featureFloats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
	featureFloats.shaderImageFloat32AtomicAdd = VK_TRUE;
	featureFloats.shaderImageFloat32Atomics = VK_TRUE;

	VkPhysicalDeviceFeatures deviceFeatures{};
	std::vector<VkDescriptorPoolSize> poolSizes(3);
	deviceFeatures.imageCubeArray = VK_TRUE;
	deviceFeatures.fullDrawIndexUint32 = VK_TRUE;
	deviceFeatures.shaderFloat64 = VK_TRUE;
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.independentBlend = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;
	deviceFeatures.geometryShader = VK_TRUE;

	poolSizes[0].descriptorCount = 100u;
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;
	poolSizes[1].descriptorCount = 100u;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = 100u;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

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
		.CreateDescriptorPool(1000u, poolSizes);
	
	res = create_swapchain_images() & res;
	res = create_framebuffers() & res;

	m_Camera.Projection.SetAspect(static_cast<float>(m_Scope.GetSwapchainExtent().width) / static_cast<float>(m_Scope.GetSwapchainExtent().height))
		.SetFOV(glm::radians(45.f))
		.SetDepthRange(1e-2f, 1e4f);

	m_ImageIndex.resize(m_ResourceCount);
	m_SwapchainSemaphores.resize(m_ResourceCount);
	m_ApplyStatusSemaphores.resize(m_ResourceCount);
	m_FrameStatusSemaphores.resize(m_ResourceCount);

	m_ApplySync.resize(m_ResourceCount);
	m_ComposeSync.resize(m_ResourceCount);
	m_PresentSync.resize(m_ResourceCount);
	m_CubemapAsync.resize(m_ResourceCount);
	m_DeferredSync.resize(m_ResourceCount);
	m_TerrainAsync.resize(m_ResourceCount);
	m_TransferAsync.resize(m_ResourceCount);
	m_BackgroundAsync.resize(m_ResourceCount);

	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_ApplySync.size(), m_ApplySync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_PresentSync.size(), m_PresentSync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetPool(), m_TerrainAsync.size(), m_TerrainAsync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_CubemapAsync.size(), m_CubemapAsync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_DeferredSync.size(), m_DeferredSync.data());
	::CreateSyncronizationStruct2(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_TransferAsync.size(), m_TransferAsync.data());
	::CreateSyncronizationStruct2(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_ComposeSync.size(), m_ComposeSync.data());
	::CreateSyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetPool(), m_BackgroundAsync.size(), m_BackgroundAsync.data());

	std::for_each(m_SwapchainSemaphores.begin(), m_SwapchainSemaphores.end(), [&, this](VkSemaphore& it) {
		res = CreateSemaphore(m_Scope.GetDevice(), &it) & res;
	});

	std::for_each(m_FrameStatusSemaphores.begin(), m_FrameStatusSemaphores.end(), [&, this](VkSemaphore& it) {
		res = CreateSemaphore(m_Scope.GetDevice(), &it) & res;
	});


	std::for_each(m_ApplyStatusSemaphores.begin(), m_ApplyStatusSemaphores.end(), [&, this](VkSemaphore& it) {
		res = CreateSemaphore(m_Scope.GetDevice(), &it) & res;
	});

	res = prepare_renderer_resources() & res;
	res = atmosphere_precompute() & res;
	res = volumetric_precompute() & res;
	res = brdf_precompute() & res;
	res = create_frame_pipelines() & res;

#ifdef INCLUDE_GUI
	m_GuiContext = ImGui::CreateContext();
	ImGui::SetCurrentContext(m_GuiContext);
	ImGui_ImplGlfw_InitForVulkan(m_GlfwWindow, false);

	std::vector<VkDescriptorPoolSize> pool_sizes =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
	};

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

	m_SSRDescriptors.resize(0);
	m_SSRPipeline.reset();
	m_SSRLUT.resize(0);

	m_TerrainCompute.reset();
	m_TerrainCompose.reset();
	m_WaterCompute.reset();
	m_BRDFLUT.reset();

	m_TerrainDrawSet.resize(0);
	m_TerrainSet.resize(0);
	m_TerrainLUT.resize(0);
	m_DiffuseIrradience.resize(0);
	m_SpecularLUT.resize(0);
	m_CubemapLUT.resize(0);
	m_WaterSet.resize(0);
	m_WaterLUT.resize(0);

	m_VolumetricsAbovePipeline.reset();
	m_VolumetricsUnderPipeline.reset();
	m_VolumetricsComposePipeline.reset();
	m_VolumetricsBetweenPipeline.reset();
	m_VolumetricsDescriptor.reset();
	m_UBOTempBuffers.resize(0);
	m_UBOSkyBuffers.resize(0);
	m_UBOBuffers.resize(0);

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
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_CubemapAsync.size(), m_CubemapAsync.data());
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_DeferredSync.size(), m_DeferredSync.data());
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_TransferAsync.size(), m_TransferAsync.data());
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetPool(), m_ComposeSync.size(), m_ComposeSync.data());
	::DestroySyncronizationStruct(m_Scope.GetDevice(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetPool(), m_BackgroundAsync.size(), m_BackgroundAsync.data());

	std::erase_if(m_SwapchainSemaphores, [&, this](VkSemaphore& it) {
		vkDestroySemaphore(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FrameStatusSemaphores, [&, this](VkSemaphore& it) {
		vkDestroySemaphore(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_ApplyStatusSemaphores, [&, this](VkSemaphore& it) {
		vkDestroySemaphore(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersHR, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	//std::erase_if(m_FramebuffersLR, [&, this](VkFramebuffer& fb) {
	//	vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
	//	return true;
	//});

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

	m_DepthAttachmentsHR.resize(0);
	m_DepthViewsHR.resize(0);

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
	m_UBOSets.resize(0);

	m_Scope.Destroy();

	vkDestroySurfaceKHR(m_VkInstance, m_Surface, VK_NULL_HANDLE);
	vkDestroyInstance(m_VkInstance, VK_NULL_HANDLE);
}

bool VulkanBase::BeginFrame()
{
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0 || glfwWindowShouldClose(m_GlfwWindow))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return false;
	}

	vkWaitForFences(m_Scope.GetDevice(), 1, &m_DeferredSync[m_ResourceIndex].Fence, VK_TRUE, UINT64_MAX);
	vkResetFences(m_Scope.GetDevice(), 1, &m_DeferredSync[m_ResourceIndex].Fence);

	assert(!m_InFrame, "Finish the frame in progress first!");

	// Udpate UBO
	{
		glm::dmat4 view_matrix = m_Camera.get_view_matrix();
		glm::dmat4 view_matrix_inverse = glm::inverse(view_matrix);
		glm::mat4 projection_matrix = m_Camera.get_projection_matrix();
		glm::mat4 projection_matrix_inverse = glm::inverse(projection_matrix);
		glm::dmat4 view_proj_matrix = static_cast<glm::dmat4>(projection_matrix) * view_matrix;
		glm::dmat4 view_proj_matrix_inverse = view_matrix_inverse * static_cast<glm::dmat4>(projection_matrix_inverse);
		glm::dvec4 CameraPositionFP64 = glm::dvec4(m_Camera.Transform.GetOffset(), 1.0);
		glm::vec4 CameraPosition = glm::vec4(m_Camera.Transform.GetOffset(), 1.0);
		glm::vec4 Sun = glm::vec4(glm::normalize(m_SunDirection), 0.0);
		glm::vec4 WorldUp = glm::vec4(glm::normalize(glm::round(glm::vec3(CameraPositionFP64))), 0.0);
		glm::vec4 CameraUp = glm::vec4(m_Camera.Transform.GetUp(), 0.0);
		glm::vec4 CameraRight = glm::vec4(m_Camera.Transform.GetRight(), 0.0);
		glm::vec4 CameraForward = glm::vec4(m_Camera.Transform.GetForward(), 0.0);
		glm::vec2 ScreenSize = glm::vec2(static_cast<float>(m_Scope.GetSwapchainExtent().width), static_cast<float>(m_Scope.GetSwapchainExtent().height));
		double CameraRadius = glm::length(CameraPositionFP64);
		float Time = glfwGetTime();

		UniformBuffer Uniform
		{
			view_proj_matrix,
			view_matrix,
			view_matrix_inverse,
			view_proj_matrix_inverse,
			CameraPositionFP64,
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
			Time
		};

		m_UBOTempBuffers[m_ResourceIndex]->Update(static_cast<void*>(&Uniform), sizeof(Uniform));
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// Start async compute to update terrain height
	if (m_TerrainCompute.get())
	{
		vkBeginCommandBuffer(m_TerrainAsync[m_ResourceIndex].Commands, &beginInfo);

		// Transfer to async queue
		if (m_FrameCount > 1)
		{
			m_TerrainLUT[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_TerrainAsync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
			m_WaterLUT[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_TerrainAsync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
		}
	
		m_TerrainLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
		m_WaterLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		m_UBOTempSets[m_ResourceIndex]->BindSet(0, m_TerrainAsync[m_ResourceIndex].Commands, *m_TerrainCompute);
		m_TerrainSet[m_ResourceIndex]->BindSet(1, m_TerrainAsync[m_ResourceIndex].Commands, *m_TerrainCompute);
		m_TerrainCompute->BindPipeline(m_TerrainAsync[m_ResourceIndex].Commands);
		vkCmdDispatch(m_TerrainAsync[m_ResourceIndex].Commands, m_TerrainDispatches, 1, 1);

		m_TerrainSet[m_ResourceIndex]->BindSet(0, m_TerrainAsync[m_ResourceIndex].Commands, *m_TerrainCompose);
		m_TerrainCompose->BindPipeline(m_TerrainAsync[m_ResourceIndex].Commands);

		for (int i = 1; i < m_TerrainLUT[0].Image->GetArrayLayers(); i++)
		{
			m_TerrainLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i - 1, 1), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
			m_TerrainCompose->PushConstants(m_TerrainAsync[m_ResourceIndex].Commands, &i, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
			vkCmdDispatch(m_TerrainAsync[m_ResourceIndex].Commands, ceil(float((m_TerrainLUT[0].Image->GetExtent().width) / 8.f)), ceil(float((m_TerrainLUT[0].Image->GetExtent().height) / 4.f)), 1);
		}

		m_TerrainLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		VkClearColorValue Color;
		Color.float32[0] = 0.0;
		vkCmdClearColorImage(m_TerrainAsync[m_ResourceIndex].Commands, m_WaterLUT[m_ResourceIndex].Image->GetImage(), VK_IMAGE_LAYOUT_GENERAL, &Color, 1, &m_WaterLUT[m_ResourceIndex].Image->GetSubResourceRange());

		m_WaterSet[m_ResourceIndex]->BindSet(0, m_TerrainAsync[m_ResourceIndex].Commands, *m_WaterCompute);
		m_WaterCompute->BindPipeline(m_TerrainAsync[m_ResourceIndex].Commands);
		vkCmdDispatch(m_TerrainAsync[m_ResourceIndex].Commands, ceil(float(m_WaterLUT[0].Image->GetExtent().width) / 8.f), ceil(float(m_WaterLUT[0].Image->GetExtent().height) / 4.f), m_WaterLUT[0].Image->GetArrayLayers());
		m_WaterLUT[m_ResourceIndex].Image->TransitionLayout(m_TerrainAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		m_TerrainLUT[m_ResourceIndex].Image->TransferOwnership(m_TerrainAsync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
		m_WaterLUT[m_ResourceIndex].Image->TransferOwnership(m_TerrainAsync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());

		vkEndCommandBuffer(m_TerrainAsync[m_ResourceIndex].Commands);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_TerrainAsync[m_ResourceIndex].Commands;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_TerrainAsync[m_ResourceIndex].Semaphore;
		vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	}

	// Begin IBL pass
	{
		vkWaitForFences(m_Scope.GetDevice(), 1, &m_ComposeSync[m_ResourceIndex].Fence, VK_TRUE, UINT64_MAX);
		vkResetFences(m_Scope.GetDevice(), 1, &m_ComposeSync[m_ResourceIndex].Fence);

		vkBeginCommandBuffer(m_CubemapAsync[m_ResourceIndex].Commands, &beginInfo);

		uint32_t mips = m_SpecularLUT[m_ResourceIndex].Image->GetMipLevelsCount();
		uint32_t X = CubeR / 8 + uint32_t(CubeR % 8 > 0);
		uint32_t Y = CubeR / 4 + uint32_t(CubeR % 4 > 0);

		m_CubemapLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_GRAPHICS_BIT);
		m_SpecularLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_GRAPHICS_BIT);
		m_DiffuseIrradience[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_GRAPHICS_BIT);

		m_CubemapPipeline->BindPipeline(m_CubemapAsync[m_ResourceIndex].Commands);
		m_CubemapDescriptors[m_ResourceIndex]->BindSet(0, m_CubemapAsync[m_ResourceIndex].Commands, *m_CubemapPipeline);
		vkCmdDispatch(m_CubemapAsync[m_ResourceIndex].Commands, X, Y, 6u);

		m_CubemapLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_GRAPHICS_BIT);
		m_CubemapLUT[m_ResourceIndex].Image->GenerateMipMaps(m_CubemapAsync[m_ResourceIndex].Commands);

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

		m_SpecularLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
		m_CubemapLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);

		VkImageCopy copy{};
		copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.dstSubresource.layerCount = 6;
		copy.dstSubresource.mipLevel = 0;
		copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.srcSubresource.layerCount = 6;
		copy.srcSubresource.mipLevel = 0;
		copy.extent = { CubeR, CubeR, 1 };
		vkCmdCopyImage(m_CubemapAsync[m_ResourceIndex].Commands, m_CubemapLUT[m_ResourceIndex].Image->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_SpecularLUT[m_ResourceIndex].Image->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

		m_CubemapLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
		m_SpecularLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
		m_SpecularLUT[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VkImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_REMAINING_MIP_LEVELS, 0, 6), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);

		m_DiffuseIrradience[m_ResourceIndex].Image->TransitionLayout(m_CubemapAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);

		vkEndCommandBuffer(m_CubemapAsync[m_ResourceIndex].Commands);

		VkSubmitInfo submitInfo{};
		std::vector<VkSemaphore> signalSemaphores = { m_CubemapAsync[m_ResourceIndex].Semaphore };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_CubemapAsync[m_ResourceIndex].Commands;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores.data();
		VkResult res = vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, VK_NULL_HANDLE);
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
		ComputePipeline* Pipeline = Re < cloudParams.BottomBound ? m_VolumetricsUnderPipeline.get() : (Re > cloudParams.TopBound ? m_VolumetricsAbovePipeline.get() : m_VolumetricsBetweenPipeline.get());

		m_UBOTempSets[m_ResourceIndex]->BindSet(0, m_BackgroundAsync[m_ResourceIndex].Commands, *Pipeline);
		m_VolumetricsDescriptor->BindSet(1, m_BackgroundAsync[m_ResourceIndex].Commands, *Pipeline);
		m_TemporalVolumetrics[m_ResourceIndex]->BindSet(2, m_BackgroundAsync[m_ResourceIndex].Commands, *Pipeline);
		Pipeline->PushConstants(m_BackgroundAsync[m_ResourceIndex].Commands, &m_ResourceIndex, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		Pipeline->BindPipeline(m_BackgroundAsync[m_ResourceIndex].Commands);
 		vkCmdDispatch(m_BackgroundAsync[m_ResourceIndex].Commands, ceil(float(m_HdrAttachmentsLR[0]->GetExtent().width) / 16), ceil(float(m_HdrAttachmentsLR[0]->GetExtent().height) / 4), 1);

		m_HdrAttachmentsLR[m_ResourceIndex]->TransitionLayout(m_BackgroundAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		m_TemporalVolumetrics[m_ResourceIndex]->BindSet(0, m_BackgroundAsync[m_ResourceIndex].Commands, *m_VolumetricsComposePipeline);
		m_VolumetricsComposePipeline->BindPipeline(m_BackgroundAsync[m_ResourceIndex].Commands);
		m_VolumetricsComposePipeline->PushConstants(m_BackgroundAsync[m_ResourceIndex].Commands, &m_ResourceIndex, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		vkCmdDispatch(m_BackgroundAsync[m_ResourceIndex].Commands, ceil(float(m_HdrAttachmentsLR[0]->GetExtent().width) / 16), ceil(float(m_HdrAttachmentsLR[0]->GetExtent().height) / 4), 1);

		m_HdrAttachmentsLR[m_ResourceIndex]->TransitionLayout(m_BackgroundAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
		// m_DepthAttachmentsLR[m_ResourceIndex]->TransitionLayout(m_BackgroundAsync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
		m_HdrAttachmentsLR[m_ResourceIndex]->TransferOwnership(VK_NULL_HANDLE, m_BackgroundAsync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
		m_DepthAttachmentsLR[m_ResourceIndex]->TransferOwnership(VK_NULL_HANDLE, m_BackgroundAsync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());

		vkEndCommandBuffer(m_BackgroundAsync[m_ResourceIndex].Commands);

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;;
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_BackgroundAsync[m_ResourceIndex].Commands;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_BackgroundAsync[m_ResourceIndex].Semaphore;
		submitInfo.waitSemaphoreCount = m_FrameCount >= m_ResourceCount ? 1 : 0;
		submitInfo.pWaitSemaphores = &m_ApplyStatusSemaphores[m_ResourceIndex];
		submitInfo.pWaitDstStageMask = &waitStage;
		vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetQueue(), 1, &submitInfo, m_BackgroundAsync[m_ResourceIndex].Fence);
	}

	// Start deferred
	{
		vkBeginCommandBuffer(m_DeferredSync[m_ResourceIndex].Commands, &beginInfo);

		// Prepare targets
		{
			if (m_TerrainCompute.get())
			{
				m_TerrainLUT[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_DeferredSync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
				m_WaterLUT[m_ResourceIndex].Image->TransferOwnership(VK_NULL_HANDLE, m_DeferredSync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
				m_TerrainLUT[WRAPR(m_ResourceIndex)].Image->TransferOwnership(m_DeferredSync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
				m_WaterLUT[WRAPR(m_ResourceIndex)].Image->TransferOwnership(m_DeferredSync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
			}

			m_HdrAttachmentsHR[m_ResourceIndex]->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
			m_DepthAttachmentsHR[m_ResourceIndex]->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
			m_BlurAttachments[2 * m_ResourceIndex]->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_GRAPHICS_BIT);
			m_BlurAttachments[2 * m_ResourceIndex + 1]->TransitionLayout(m_DeferredSync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_GRAPHICS_BIT);
		}

		VkBufferCopy region{};
		region.size = sizeof(UniformBuffer);
		vkCmdCopyBuffer(m_DeferredSync[m_ResourceIndex].Commands, m_UBOTempBuffers[m_ResourceIndex]->GetBuffer(), m_UBOBuffers[m_ResourceIndex]->GetBuffer(), 1, &region);

#ifdef INCLUDE_GUI
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
#endif

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

		VkSubmitInfo submitInfo{};
		std::vector<VkPipelineStageFlags> waitStages = { };
		std::vector<VkSemaphore> waitSemaphores = { };
		waitSemaphores.reserve(4);
		waitStages.reserve(4);

		std::vector<VkSemaphore> signalSemaphores = { m_DeferredSync[m_ResourceIndex].Semaphore };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		if (m_TerrainCompute.get())
		{
			waitSemaphores.push_back(m_TerrainAsync[m_ResourceIndex].Semaphore);
			waitStages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}

		submitInfo.waitSemaphoreCount = waitSemaphores.size();
		submitInfo.pWaitSemaphores = waitSemaphores.data();
		submitInfo.pWaitDstStageMask = waitStages.data();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_DeferredSync[m_ResourceIndex].Commands;
		submitInfo.signalSemaphoreCount = signalSemaphores.size();
		submitInfo.pSignalSemaphores = signalSemaphores.data();
		VkResult res = vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, m_DeferredSync[m_ResourceIndex].Fence);
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// Draw High Resolution Objects
	{
		vkBeginCommandBuffer(m_ComposeSync[m_ResourceIndex].Commands, &beginInfo);

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

		vkEndCommandBuffer(m_ComposeSync[m_ResourceIndex].Commands);

		VkSubmitInfo submitInfo{};
		std::vector<VkPipelineStageFlags> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
		std::vector<VkSemaphore> waitSemaphores = { m_DeferredSync[m_ResourceIndex].Semaphore, m_CubemapAsync[m_ResourceIndex].Semaphore };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// if we just started drawing, we should skip image present semaphore as it has not yet been signaled
		if (m_FrameCount >= m_ResourceCount)
		{
			waitSemaphores.push_back(m_PresentSync[m_ResourceIndex].Semaphore);
			waitStages.push_back(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
		}

		submitInfo.waitSemaphoreCount = waitSemaphores.size();
		submitInfo.pWaitSemaphores = waitSemaphores.data();
		submitInfo.pWaitDstStageMask = waitStages.data();
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_ComposeSync[m_ResourceIndex].Semaphore;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_ComposeSync[m_ResourceIndex].Commands;
		VkResult res = vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, m_ComposeSync[m_ResourceIndex].Fence);
	}

	// Bloom pass
	{
		vkWaitForFences(m_Scope.GetDevice(), 1, &m_ApplySync[m_ResourceIndex].Fence, VK_TRUE, UINT64_MAX);
		vkResetFences(m_Scope.GetDevice(), 1, &m_ApplySync[m_ResourceIndex].Fence);

		vkBeginCommandBuffer(m_ApplySync[m_ResourceIndex].Commands, &beginInfo);

		m_HdrAttachmentsLR[m_ResourceIndex]->TransferOwnership(VK_NULL_HANDLE, m_ApplySync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
		m_DepthAttachmentsLR[m_ResourceIndex]->TransferOwnership(VK_NULL_HANDLE, m_ApplySync[m_ResourceIndex].Commands, m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());

		VkBufferCopy region{};
		region.size = sizeof(UniformBuffer);
		vkCmdCopyBuffer(m_ApplySync[m_ResourceIndex].Commands, m_UBOTempBuffers[WRAPL(m_ResourceIndex)]->GetBuffer(), m_UBOSkyBuffers[m_ResourceIndex]->GetBuffer(), 1, &region);

		uint32_t X = ceil(float(m_Scope.GetSwapchainExtent().width) / 8.f);
		uint32_t Y = ceil(float(m_Scope.GetSwapchainExtent().height) / 4.f);

		m_DepthAttachmentsLR[WRAPR(m_ResourceIndex)]->TransitionLayout(m_ApplySync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_GRAPHICS_BIT);

		m_BlendingPipeline->BindPipeline(m_ApplySync[m_ResourceIndex].Commands);
		m_BlendingDescriptors[m_ResourceIndex]->BindSet(0, m_ApplySync[m_ResourceIndex].Commands, *m_BlendingPipeline);
		vkCmdDispatch(m_ApplySync[m_ResourceIndex].Commands, X, Y, 1);

		m_HdrAttachmentsHR[m_ResourceIndex]->TransitionLayout(m_ApplySync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
		m_DepthAttachmentsLR[WRAPR(m_ResourceIndex)]->TransitionLayout(m_ApplySync[m_ResourceIndex].Commands, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);

		m_HdrAttachmentsLR[WRAPR(m_ResourceIndex)]->TransferOwnership(m_ApplySync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
		m_DepthAttachmentsLR[WRAPR(m_ResourceIndex)]->TransferOwnership(m_ApplySync[m_ResourceIndex].Commands, VK_NULL_HANDLE, m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());

		vkEndCommandBuffer(m_ApplySync[m_ResourceIndex].Commands);

		VkSubmitInfo submitInfo{};
		std::vector<VkPipelineStageFlags> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
		std::vector<VkSemaphore> waitSemaphores = { m_ComposeSync[m_ResourceIndex].Semaphore, m_BackgroundAsync[m_ResourceIndex].Semaphore };
		std::vector<VkSemaphore> signalSemaphores = { m_ApplySync[m_ResourceIndex].Semaphore, m_ApplyStatusSemaphores[m_ResourceIndex] };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = waitSemaphores.size();
		submitInfo.pWaitSemaphores = waitSemaphores.data();
		submitInfo.pWaitDstStageMask = waitStages.data();
		submitInfo.signalSemaphoreCount = signalSemaphores.size();
		submitInfo.pSignalSemaphores = signalSemaphores.data();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_ApplySync[m_ResourceIndex].Commands;
		VkResult res = vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, m_ApplySync[m_ResourceIndex].Fence);
	}

	// Post processing and present
	{
		vkWaitForFences(m_Scope.GetDevice(), 1, &m_PresentSync[m_ResourceIndex].Fence, VK_TRUE, UINT64_MAX);
		vkResetFences(m_Scope.GetDevice(), 1, &m_PresentSync[m_ResourceIndex].Fence);

		vkAcquireNextImageKHR(m_Scope.GetDevice(), m_Scope.GetSwapchain(), 0, m_SwapchainSemaphores[m_ResourceIndex], VK_NULL_HANDLE, &m_ImageIndex[m_ResourceIndex]);
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

		const int Radius = 5;
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

			}

			vkCmdPipelineBarrier(m_PresentSync[m_ResourceIndex].Commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, barrier.size(), barrier.data());
		}

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

		m_PostProcessDescriptors[m_ResourceIndex]->BindSet(0, m_PresentSync[m_ResourceIndex].Commands, *m_PostProcessPipeline);
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
		VkSubmitInfo submitInfo{};
		std::vector<VkSemaphore> waitSemaphores = { m_SwapchainSemaphores[m_ResourceIndex], m_ApplySync[m_ResourceIndex].Semaphore };
		std::vector<VkSemaphore> signalSemaphores = { m_PresentSync[m_ResourceIndex].Semaphore, m_FrameStatusSemaphores[m_ResourceIndex] };

		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = waitSemaphores.size();
		submitInfo.pWaitSemaphores = waitSemaphores.data();
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_PresentSync[m_ResourceIndex].Commands;
		submitInfo.signalSemaphoreCount = signalSemaphores.size();
		submitInfo.pSignalSemaphores = signalSemaphores.data();

		VkResult res = vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, m_PresentSync[m_ResourceIndex].Fence);
		assert(res != VK_ERROR_DEVICE_LOST);
	}

	// Present final image
	{
		std::vector<VkSemaphore> waitSemaphores = { m_FrameStatusSemaphores[m_ResourceIndex] };
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

	m_DepthAttachmentsHR.resize(0);
	m_DepthViewsHR.resize(0);

	m_HdrAttachmentsLR.resize(0);
	m_HdrViewsLR.resize(0);

	m_DepthAttachmentsLR.resize(0);
	m_DepthViewsLR.resize(0);

	m_TemporalVolumetrics.resize(0);
	m_SSRLUT.resize(0);

	m_SwapchainImages.resize(0);

	m_Scope.RecreateSwapchain(m_Surface);

	if (m_Scope.GetSwapchain() == VK_NULL_HANDLE)
		return;

	create_swapchain_images();
	create_framebuffers();
	create_frame_pipelines();

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
	cloudParams.BottomSmoothnessFactor = glm::mix(0.05, 0.5, cloudParams.CoverageSq);
	cloudParams.LightIntensity = glm::mix(1.0, 0.05, cloudParams.CoverageSq);
	cloudParams.Ambient = glm::mix(0.01, 0.005, cloudParams.CoverageSq);
	cloudParams.Wind = settings.WindSpeed * 0.01;
	cloudParams.Density = settings.Density;
	cloudParams.TopBound = Rct;
	cloudParams.BottomBound = Rcb + (Rct - Rcb) * (0.5 - glm::min(cloudParams.Coverage, 0.5f));
	cloudParams.BoundDelta = cloudParams.TopBound - cloudParams.BottomBound;

	m_CloudLayer->Update(&cloudParams, sizeof(CloudParameters));
}

void VulkanBase::SetTerrainLayerSettings(float Scale, int Count, TerrainLayerProfile* settings)
{
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

	m_SSRLUT.resize(m_ResourceCount);

	m_SwapchainImages.resize(imagesCount);
	m_SwapchainViews.resize(imagesCount);

	m_DepthAttachmentsHR.resize(m_ResourceCount);
	m_DepthViewsHR.resize(m_ResourceCount);

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

		hdrInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		m_HdrAttachmentsHR[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_HdrViewsHR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_HdrAttachmentsHR[i]);

		m_SSRLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_SSRLUT[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_SSRLUT[i].Image);

		hdrInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		m_BlurAttachments[2 * i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_BlurViews[2 * i] = std::make_unique<VulkanImageView>(m_Scope, *m_BlurAttachments[2 * i]);

		m_BlurAttachments[2 * i + 1] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_BlurViews[2 * i + 1] = std::make_unique<VulkanImageView>(m_Scope, *m_BlurAttachments[2 * i + 1]);

		hdrInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		hdrInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		m_DeferredAttachments[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_DeferredViews[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DeferredAttachments[i]);

		hdrInfo.format = VK_FORMAT_R8G8B8A8_SNORM;
		m_NormalAttachments[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_NormalViews[i] = std::make_unique<VulkanImageView>(m_Scope, *m_NormalAttachments[i]);

		depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		m_DepthAttachmentsHR[i] = std::make_unique<VulkanImage>(m_Scope, depthInfo, allocCreateInfo);
		m_DepthViewsHR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DepthAttachmentsHR[i]);

		hdrInfo.extent.width /= LRr;
		hdrInfo.extent.height /= LRr;
		hdrInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		hdrInfo.format = m_Scope.GetHDRFormat();
		m_HdrAttachmentsLR[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_HdrViewsLR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_HdrAttachmentsLR[i]);

		depthInfo.extent.width /= LRr;
		depthInfo.extent.height /= LRr;
		depthInfo.format = VK_FORMAT_R32_SFLOAT;
		depthInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		m_DepthAttachmentsLR[i] = std::make_unique<VulkanImage>(m_Scope, depthInfo, allocCreateInfo);
		m_DepthViewsLR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DepthAttachmentsLR[i]);
	}

	return res;
}

VkBool32 VulkanBase::create_framebuffers()
{
	assert(m_ResourceCount > 0 && m_Scope.GetRenderPass() != VK_NULL_HANDLE);

	m_FramebuffersHR.resize(m_ResourceCount);
	// m_FramebuffersLR.resize(m_ResourceCount);
	m_FramebuffersCP.resize(m_ResourceCount);
	m_FramebuffersPP.resize(m_ResourceCount);

	VkBool32 res = 1;
	for (size_t i = 0; i < m_ResourceCount; i++)
	{
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetRenderPass(), { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 }, { m_HdrViewsHR[i]->GetImageView(), m_NormalViews[i]->GetImageView(),m_DeferredViews[i]->GetImageView(), m_DepthViewsHR[i]->GetImageView() }, &m_FramebuffersHR[i]) & res;
		// res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetLowResRenderPass(), { m_Scope.GetSwapchainExtent().width / LRr, m_Scope.GetSwapchainExtent().height / LRr, 1 }, { m_HdrViewsLR[i]->GetImageView(), m_DepthViewsLR[i]->GetImageView()}, &m_FramebuffersLR[i]) & res;
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetCompositionPass(), { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 }, { m_HdrViewsHR[i]->GetImageView() }, &m_FramebuffersCP[i]);
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetPostProcessPass(), { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 }, { m_SwapchainViews[i] }, &m_FramebuffersPP[i]);
	}

	return res;
}

VkBool32 VulkanBase::create_frame_pipelines()
{
	VkBool32 res = 1;

	m_BlendingDescriptors.resize(m_ResourceCount);
	m_CompositionDescriptors.resize(m_ResourceCount);
	m_PostProcessDescriptors.resize(m_ResourceCount);
	m_TemporalVolumetrics.resize(m_ResourceCount);
	m_BlurDescriptors.resize(m_ResourceCount);
	m_SSRDescriptors.resize(m_ResourceCount);

	VkSampler SamplerPoint = m_Scope.GetSampler(ESamplerType::PointClamp, 1);
	VkSampler SamplerLinear = m_Scope.GetSampler(ESamplerType::LinearClamp, 1);
	VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::LinearRepeat, 1);

	for (size_t i = 0; i < m_ResourceCount; i++)
	{
		m_CompositionDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsHR[i]->GetImageView(), SamplerPoint, VK_IMAGE_LAYOUT_GENERAL)
			.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, m_NormalViews[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, m_DeferredViews[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(4, VK_SHADER_STAGE_FRAGMENT_BIT, m_DepthViewsHR[i]->GetImageView(), SamplerPoint)
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

		m_SSRDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_DepthViewsHR[WRAPL(i)]->GetImageView(), SamplerPoint)
			.Allocate(m_Scope);

		m_TemporalVolumetrics[i] = DescriptorSetDescriptor()
			.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_HdrViewsLR[i]->GetImageView())
			.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_DepthViewsLR[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_HdrViewsLR[WRAPL(i)]->GetImageView(), SamplerPoint)
			.AddUniformBuffer(3, VK_SHADER_STAGE_COMPUTE_BIT, *m_UBOSkyBuffers[i])
			.Allocate(m_Scope);

		m_BlendingDescriptors[i] = DescriptorSetDescriptor()
			.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_HdrViewsHR[i]->GetImageView())
			.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_DepthViewsLR[WRAPR(i)]->GetImageView())
			.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_HdrViewsLR[i]->GetImageView(), SamplerLinear)
			.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, m_DepthViewsHR[i]->GetImageView(), SamplerLinear)
			.Allocate(m_Scope);
	}

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

	m_SSRPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_SSRDescriptors[0]->GetLayout())
		.SetShaderName("ssreflection_comp")
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
	m_UBOSets.resize(m_ResourceCount);

	for (uint32_t i = 0; i < m_ResourceCount; i++)
	{
		uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		m_UBOBuffers[i] = std::make_unique<Buffer>(m_Scope, uboInfo, uboAllocCreateInfo1);
		m_UBOSkyBuffers[i] = std::make_unique<Buffer>(m_Scope, uboInfo, uboAllocCreateInfo1);

		uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		m_UBOTempBuffers[i] = std::make_unique<Buffer>(m_Scope, uboInfo, uboAllocCreateInfo2);
	}
	 
	for (uint32_t i = 0; i < m_ResourceCount; i++)
	{
		m_UBOSets[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_ALL, *m_UBOBuffers[i])
			.Allocate(m_Scope);

		m_UBOTempSets[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_ALL, *m_UBOTempBuffers[i])
			.Allocate(m_Scope);
	}

	m_DefaultWhite = std::make_shared<VulkanTexture>();
	m_DefaultWhite->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_SRGB, std::byte(255u), std::byte(255u), std::byte(255u), std::byte(255u));
	m_DefaultWhite->View = std::make_unique<VulkanImageView>(m_Scope, *m_DefaultWhite->Image);

	m_DefaultBlack = std::make_shared<VulkanTexture>();
	m_DefaultBlack->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(0u));
	m_DefaultBlack->View = std::make_unique<VulkanImageView>(m_Scope, *m_DefaultBlack->Image);

	m_DefaultNormal = std::make_shared<VulkanTexture>();
	m_DefaultNormal->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(127u), std::byte(127u), std::byte(255u), std::byte(255u));
	m_DefaultNormal->View = std::make_unique<VulkanImageView>(m_Scope, *m_DefaultNormal->Image);

	m_DefaultARM = std::make_shared<VulkanTexture>();
	m_DefaultARM->Image = GRNoise::GenerateSolidColor(m_Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_SRGB, std::byte(255u), std::byte(255u), std::byte(0u), std::byte(255u));
	m_DefaultARM->View = std::make_unique<VulkanImageView>(m_Scope, *m_DefaultARM->Image);

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

entt::entity VulkanBase::_constructShape(entt::entity ent, entt::registry& registry, const GR::Shapes::GeoClipmap& shape) const
{
	PBRObject& gro = registry.emplace_or_replace<PBRObject>(ent);
	gro.mesh = shape.Generate(m_Scope);

	const_cast<VulkanBase*>(this)->terrain_init(*gro.mesh->GetVertexBuffer(), shape);

	gro.descriptorSet = create_terrain_set(*m_DefaultWhite->View, *m_DefaultNormal->View, *m_DefaultARM->View);
	gro.pipeline = create_terrain_pipeline(*gro.descriptorSet, shape);

	registry.emplace_or_replace<GR::Components::AlbedoMap>(ent, m_DefaultWhite, &gro.dirty);
	registry.emplace_or_replace<GR::Components::NormalDisplacementMap>(ent, m_DefaultNormal, &gro.dirty);
	registry.emplace_or_replace<GR::Components::AORoughnessMetallicMapTransmittance>(ent, m_DefaultWhite, &gro.dirty);

	return ent;
}

entt::entity VulkanBase::_constructShape(entt::entity ent, entt::registry& registry, const GR::Shapes::Shape& shape) const
{
	PBRObject& gro = registry.emplace_or_replace<PBRObject>(ent);
	gro.descriptorSet = create_pbr_set(*m_DefaultWhite->View, *m_DefaultNormal->View, *m_DefaultARM->View);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);
	gro.mesh = shape.Generate(m_Scope);

	registry.emplace_or_replace<GR::Components::AlbedoMap>(ent, m_DefaultWhite, &gro.dirty);
	registry.emplace_or_replace<GR::Components::NormalDisplacementMap>(ent, m_DefaultNormal, &gro.dirty);
	registry.emplace_or_replace<GR::Components::AORoughnessMetallicMapTransmittance>(ent, m_DefaultWhite, &gro.dirty);

	return ent;
}

void VulkanBase::_drawTerrain(const PBRObject& gro, const PBRConstants& constants) const
{
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0)
		return;

	assert(m_InFrame, "Call BeginFrame first!");

	const VkCommandBuffer& cmd = m_DeferredSync[m_ResourceIndex].Commands;
	const VkDeviceSize offsets[] = { 0 };

	m_GrassPipeline->BindPipeline(cmd);
	m_UBOSets[m_ResourceIndex]->BindSet(0, cmd, *m_GrassPipeline);
	m_GrassSet[m_ResourceIndex]->BindSet(1, cmd, *m_GrassPipeline);
	gro.descriptorSet->BindSet(2, cmd, *m_GrassPipeline);

	uint32_t firstRing = m_TerrainLUT[0].Image->GetExtent().width * m_TerrainLUT[0].Image->GetExtent().height;
	uint32_t nextRings = m_TerrainLUT[0].Image->GetExtent().width * m_TerrainLUT[0].Image->GetExtent().height - glm::ceil(float(m_TerrainLUT[0].Image->GetExtent().width) / 2.0) * glm::ceil(float(m_TerrainLUT[0].Image->GetExtent().height) / 2.0);
	
	float Lod = 0.0;
	m_GrassPipeline->PushConstants(cmd, &Lod, sizeof(float), 0, VK_SHADER_STAGE_VERTEX_BIT);
	vkCmdDraw(m_DeferredSync[m_ResourceIndex].Commands, 27, firstRing, 0, 0);
	
	if (m_TerrainLUT[m_ResourceIndex].Image->GetArrayLayers() > 0)
	{
		Lod = 1.0;
		m_GrassPipeline->PushConstants(cmd, &Lod, sizeof(float), 0, VK_SHADER_STAGE_VERTEX_BIT);
		vkCmdDraw(m_DeferredSync[m_ResourceIndex].Commands, 15, 2 * nextRings, 0, m_TerrainLUT[0].Image->GetExtent().width * m_TerrainLUT[0].Image->GetExtent().height);
	}
	
	if (m_TerrainLUT[m_ResourceIndex].Image->GetArrayLayers() > 1)
	{
		Lod = 2.0;
		m_GrassPipeline->PushConstants(cmd, &Lod, sizeof(float), 0, VK_SHADER_STAGE_VERTEX_BIT);
		vkCmdDraw(m_DeferredSync[m_ResourceIndex].Commands, 3, 3 * nextRings, 0, nextRings + firstRing);
	}

	m_UBOSets[m_ResourceIndex]->BindSet(0, cmd, *gro.pipeline);

	gro.descriptorSet->BindSet(1, cmd, *gro.pipeline);
	m_TerrainDrawSet[m_ResourceIndex]->BindSet(2, cmd, *gro.pipeline);

	gro.pipeline->PushConstants(cmd, &constants.Offset, PBRConstants::VertexSize(), 0u, VK_SHADER_STAGE_VERTEX_BIT);
	gro.pipeline->PushConstants(cmd, &constants.Color, PBRConstants::FragmentSize(), PBRConstants::VertexSize(), VK_SHADER_STAGE_FRAGMENT_BIT);
	gro.pipeline->BindPipeline(cmd);
	
	// vkCmdBindVertexBuffers(cmd, 0, 1, &gro.mesh->GetVertexBuffer()->GetBuffer(), offsets);
	vkCmdBindVertexBuffers(cmd, 0, 1, &TerrainVBs[m_ResourceIndex]->GetBuffer(), offsets);
	vkCmdBindIndexBuffer(cmd, gro.mesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, gro.mesh->GetIndicesCount(), 1, 0, 0, 0);
}

void VulkanBase::_drawObject(const PBRObject& gro, const PBRConstants& constants) const
{
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0)
		return;

	assert(m_InFrame, "Call BeginFrame first!");

	const VkCommandBuffer& cmd = m_DeferredSync[m_ResourceIndex].Commands;
	const VkDeviceSize offsets[] = { 0 };

	m_UBOSets[m_ResourceIndex]->BindSet(0, cmd, *gro.pipeline);
	gro.descriptorSet->BindSet(1, cmd, *gro.pipeline);
	gro.pipeline->PushConstants(cmd, &constants.Offset, PBRConstants::VertexSize(), 0u, VK_SHADER_STAGE_VERTEX_BIT);
	gro.pipeline->PushConstants(cmd, &constants.Color, PBRConstants::FragmentSize(), PBRConstants::VertexSize(), VK_SHADER_STAGE_FRAGMENT_BIT);
	gro.pipeline->BindPipeline(cmd);

	vkCmdBindVertexBuffers(cmd, 0, 1, &gro.mesh->GetVertexBuffer()->GetBuffer(), offsets);
	vkCmdBindIndexBuffer(cmd, gro.mesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, gro.mesh->GetIndicesCount(), 1, 0, 0, 0);
}

void VulkanBase::_updateObject(entt::entity ent, entt::registry& registry) const
{
	VulkanTexture* albedo = static_cast<VulkanTexture*>(registry.get<GR::Components::AlbedoMap>(ent).Get().get());
	VulkanTexture* nh = static_cast<VulkanTexture*>(registry.get<GR::Components::NormalDisplacementMap>(ent).Get().get());
	VulkanTexture* arm = static_cast<VulkanTexture*>(registry.get<GR::Components::AORoughnessMetallicMapTransmittance>(ent).Get().get());

	PBRObject& gro = registry.get<PBRObject>(ent);
	gro.descriptorSet = create_pbr_set(*albedo->View, *nh->View, *arm->View);
	gro.dirty = false;
}

void VulkanBase::_updateTerrain(entt::entity ent, entt::registry& registry) const
{
	VulkanTexture* albedo = static_cast<VulkanTexture*>(registry.get<GR::Components::AlbedoMap>(ent).Get().get());
	VulkanTexture* nh = static_cast<VulkanTexture*>(registry.get<GR::Components::NormalDisplacementMap>(ent).Get().get());
	VulkanTexture* arm = static_cast<VulkanTexture*>(registry.get<GR::Components::AORoughnessMetallicMapTransmittance>(ent).Get().get());

	PBRObject& gro = registry.get<PBRObject>(ent);
	gro.descriptorSet = create_terrain_set(*albedo->View, *nh->View, *arm->View);
	gro.dirty = false;
}

std::unique_ptr<DescriptorSet> VulkanBase::create_pbr_set(const VulkanImageView& albedo
	, const VulkanImageView& nh
	, const VulkanImageView& arm) const
{
	return DescriptorSetDescriptor()
		.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, albedo.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, albedo.GetSubresourceRange().levelCount))
		.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, nh.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, nh.GetSubresourceRange().levelCount))
		.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, arm.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, arm.GetSubresourceRange().levelCount))
		.Allocate(m_Scope);
}

std::unique_ptr<GraphicsPipeline> VulkanBase::create_pbr_pipeline(const DescriptorSet& set) const
{
	auto vertAttributes = MeshVertex::getAttributeDescriptions();
	auto vertBindings = MeshVertex::getBindingDescription();

	return GraphicsPipelineDescriptor()
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.SetBlendAttachments(3, nullptr)
		.SetVertexInputBindings(1, &vertBindings)
		.SetVertexAttributeBindings(vertAttributes.size(), vertAttributes.data())
		.SetShaderStage("default_vert", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("default_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(set.GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<uint32_t>(PBRConstants::VertexSize()) })
		.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(PBRConstants::VertexSize()), static_cast<uint32_t>(PBRConstants::FragmentSize()) })
		.AddSpecializationConstant(0, Rg, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddSpecializationConstant(1, Rt, VK_SHADER_STAGE_FRAGMENT_BIT)
		.Construct(m_Scope);
}

std::unique_ptr<DescriptorSet> VulkanBase::create_terrain_set(const VulkanImageView& albedo
	, const VulkanImageView& nh
	, const VulkanImageView& arm) const
{
	return DescriptorSetDescriptor()
		.AddImageSampler(0, VK_SHADER_STAGE_FRAGMENT_BIT, albedo.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, albedo.GetSubresourceRange().levelCount))
		.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, nh.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, nh.GetSubresourceRange().levelCount))
		.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, arm.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, arm.GetSubresourceRange().levelCount))
		// .AddImageSampler(4, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TerrainLUT.View->GetImageView(), SamplerRepeat)
		.Allocate(m_Scope);
}

std::unique_ptr<GraphicsPipeline> VulkanBase::create_terrain_pipeline(const DescriptorSet& set, const GR::Shapes::GeoClipmap& shape) const
{
	auto vertAttributes = TerrainVertex::getAttributeDescriptions();
	auto vertBindings = TerrainVertex::getBindingDescription();

	return GraphicsPipelineDescriptor()
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.SetBlendAttachments(3, nullptr)
		.SetVertexInputBindings(1, &vertBindings)
		.SetVertexAttributeBindings(vertAttributes.size(), vertAttributes.data())
		.SetShaderStage("terrain_vert", VK_SHADER_STAGE_VERTEX_BIT)
		// .SetShaderStage("terrain_geom", VK_SHADER_STAGE_GEOMETRY_BIT)
		.SetShaderStage("terrain_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(set.GetLayout())
		.AddDescriptorLayout(m_TerrainDrawSet[0]->GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<uint32_t>(PBRConstants::VertexSize()) })
		.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(PBRConstants::VertexSize()), static_cast<uint32_t>(PBRConstants::FragmentSize()) })
		.AddSpecializationConstant(0, Rg, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(1, Rt, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(2, shape.m_Scale, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(3, shape.m_MinHeight, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(4, shape.m_MaxHeight, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(5, shape.m_NoiseSeed, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(6, float(glm::ceil(float(shape.m_Rings) / 3.f) +  1.f), VK_SHADER_STAGE_VERTEX_BIT)
		// .SetPolygonMode(VK_POLYGON_MODE_LINE)
		.Construct(m_Scope);
}
#pragma endregion

#pragma region Precompute
VkBool32 VulkanBase::brdf_precompute()
{
	std::vector<uint32_t> queueFamilies = { m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex() };
	std::sort(queueFamilies.begin(), queueFamilies.end());
	queueFamilies.resize(std::distance(queueFamilies.begin(), std::unique(queueFamilies.begin(), queueFamilies.end())));

	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	m_DiffuseIrradience.resize(m_ResourceCount);
	m_SpecularLUT.resize(m_ResourceCount);
	m_CubemapLUT.resize(m_ResourceCount);

	for (size_t i = 0; i < m_ResourceCount; i++)
	{
		VkImageCreateInfo hdrInfo{};
		hdrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		hdrInfo.format = m_Scope.GetHDRFormat();
		hdrInfo.arrayLayers = 6;
		hdrInfo.extent = { CubeR, CubeR, 1 };
		hdrInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		hdrInfo.imageType = VK_IMAGE_TYPE_2D;
		hdrInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		hdrInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		hdrInfo.mipLevels = 1;
		hdrInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		hdrInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		hdrInfo.queueFamilyIndexCount = queueFamilies.size();
		hdrInfo.pQueueFamilyIndices = queueFamilies.data();
		hdrInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		m_DiffuseIrradience[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_DiffuseIrradience[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_DiffuseIrradience[i].Image);

		hdrInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		hdrInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(CubeR))) + 1;
		m_SpecularLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_SpecularLUT[i].Views.emplace_back(std::make_unique<VulkanImageView>(m_Scope, *m_SpecularLUT[i].Image));

		m_CubemapLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_CubemapLUT[i].Views.emplace_back(std::make_unique<VulkanImageView>(m_Scope, *m_CubemapLUT[i].Image));

		m_SpecularLUT[i].Views.reserve(hdrInfo.mipLevels + 1);
		m_CubemapLUT[i].Views.reserve(hdrInfo.mipLevels + 1);
		VkImageSubresourceRange subRes = m_SpecularLUT[i].Image->GetSubResourceRange();
		for (uint32_t j = 0; j < hdrInfo.mipLevels; j++)
		{
			subRes.baseMipLevel = j;
			subRes.levelCount = 1u;
			m_SpecularLUT[i].Views.emplace_back(std::make_unique<VulkanImageView>(m_Scope, *m_SpecularLUT[i].Image, subRes));
			m_CubemapLUT[i].Views.emplace_back(std::make_unique<VulkanImageView>(m_Scope, *m_CubemapLUT[i].Image, subRes));
		}
	}

	std::unique_ptr<GraphicsPipeline> m_IntegrationPipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("brdf_integrate_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetRenderPass(m_Scope.GetSimplePass(), 0)
		.Construct(m_Scope);

	VkImageCreateInfo bimageInfo{};
	bimageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	bimageInfo.format = m_Scope.GetHDRFormat();
	bimageInfo.arrayLayers = 1;
	bimageInfo.extent = { 512, 512, 1 };
	bimageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	bimageInfo.imageType = VK_IMAGE_TYPE_2D;
	bimageInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	bimageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	bimageInfo.mipLevels = 1;
	bimageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	bimageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bimageInfo.queueFamilyIndexCount = queueFamilies.size();
	bimageInfo.pQueueFamilyIndices = queueFamilies.data();

	m_BRDFLUT.Image = std::make_unique<VulkanImage>(m_Scope, bimageInfo, allocCreateInfo);
	m_BRDFLUT.View = std::make_unique<VulkanImageView>(m_Scope, *m_BRDFLUT.Image);

	VkFramebuffer Framebuffer;
	CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetCubemapPass(), { m_BRDFLUT.Image->GetExtent().width, m_BRDFLUT.Image->GetExtent().height, 1 }, { m_BRDFLUT.View->GetImageView() }, &Framebuffer);

	VkCommandBuffer cmd;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers(1, &cmd);
	vkBeginCommandBuffer(cmd, &beginInfo);

	{
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = Framebuffer;
		renderPassInfo.renderPass = m_Scope.GetSimplePass();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { m_BRDFLUT.Image->GetExtent().width, m_BRDFLUT.Image->GetExtent().height };

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(m_BRDFLUT.Image->GetExtent().width);
		viewport.height = static_cast<float>(m_BRDFLUT.Image->GetExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { m_BRDFLUT.Image->GetExtent().width, m_BRDFLUT.Image->GetExtent().height };
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_IntegrationPipeline->BindPipeline(cmd);
		vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdEndRenderPass(cmd);
	}

	vkEndCommandBuffer(cmd);

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	vkDestroyFramebuffer(m_Scope.GetDevice(), Framebuffer, VK_NULL_HANDLE);

	m_CubemapDescriptors.resize(m_ResourceCount);
	m_DiffuseDescriptors.resize(m_ResourceCount);
	m_ConvolutionDescriptors.resize(m_ResourceCount);
	m_SpecularDescriptors.resize(m_SpecularLUT[0].Image->GetMipLevelsCount() * m_ResourceCount);
	m_CubemapMipDescriptors.resize(m_SpecularLUT[0].Image->GetMipLevelsCount() * m_ResourceCount);

	struct
	{
		int Count       = 0;
		float ColorNorm = 0.0;
		std::vector<glm::vec4> data = {};
	} diffuseData;
	diffuseData.data.reserve(663);

	float sampleDelta = 0.25;
	for (float phi = 0.0; phi < 2.0 * glm::pi<float>(); phi += sampleDelta)
	{
		float sinPhi = sin(phi);
		float cosPhi = cos(phi);

		for (float theta = 0.0; theta < 0.5 * glm::pi<float>(); theta += sampleDelta)
		{
			float sinTheta = sin(theta);
			float cosTheta = cos(theta);

			diffuseData.data.emplace_back(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta, sinTheta * cosTheta);
			diffuseData.Count++;
		}
	}
	diffuseData.ColorNorm = glm::pi<float>() / float(diffuseData.Count);

	VmaAllocationCreateInfo bufallocCreateInfo{};
	bufallocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkBufferCreateInfo bufInfo{};
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufInfo.size = sizeof(glm::vec4) * diffuseData.data.size();
	bufInfo.queueFamilyIndexCount = queueFamilies.size();
	bufInfo.pQueueFamilyIndices = queueFamilies.data();
	bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	m_DiffusePrecompute = std::make_unique<Buffer>(m_Scope, bufInfo, bufallocCreateInfo);
	m_DiffusePrecompute->Update(diffuseData.data.data(), sizeof(glm::vec4)* diffuseData.data.size());

	std::vector<glm::vec4> specSamples;
	const int specSamplesCount = 32;
	specSamples.reserve(specSamplesCount);
	for (int i = 0; i < specSamplesCount; i++)
	{
		glm::vec4 sample;
		sample.x = float(i) / float(specSamplesCount);

		// radical inverse
		uint32_t N = static_cast<uint32_t>(specSamplesCount);
		N = (N << 16u) | (N >> 16u);
		N = ((N & 0x55555555u) << 1u) | ((N & 0xAAAAAAAAu) >> 1u);
		N = ((N & 0x33333333u) << 2u) | ((N & 0xCCCCCCCCu) >> 2u);
		N = ((N & 0x0F0F0F0Fu) << 4u) | ((N & 0xF0F0F0F0u) >> 4u);
		N = ((N & 0x00FF00FFu) << 8u) | ((N & 0xFF00FF00u) >> 8u);
		sample.y = float(N) * 2.3283064365386963e-10; // / 0x100000000
		sample.z = 4.0 * glm::pi<float>() / float(6.0 * CubeR * CubeR);

		specSamples.emplace_back(sample);
	}
	bufInfo.size = sizeof(glm::vec4) * specSamples.size();
	m_SpecularPrecompute = std::make_unique<Buffer>(m_Scope, bufInfo, bufallocCreateInfo);
	m_SpecularPrecompute->Update(specSamples.data(), sizeof(glm::vec4)* specSamples.size());

	VkSampler SamplerPoint = m_Scope.GetSampler(ESamplerType::PointClamp, 1);
	VkSampler SamplerLinear = m_Scope.GetSampler(ESamplerType::LinearClamp, 1);
	VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::LinearRepeat, 1);
	for (size_t i = 0; i < m_ResourceCount; i++)
	{
		m_CubemapDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_UBOBuffers[i])
			.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_CubemapLUT[i].Views[0]->GetImageView())
			.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView(), SamplerLinear)
			.Allocate(m_Scope);

		m_ConvolutionDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, m_CubemapLUT[i].Views[0]->GetImageView(), m_Scope.GetSampler(ESamplerType::LinearClamp, m_CubemapLUT[i].Views[0]->GetSubresourceRange().levelCount))
			.Allocate(m_Scope);

		m_DiffuseDescriptors[i] = DescriptorSetDescriptor()
			.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_DiffuseIrradience[i].View->GetImageView())
			.AddStorageBuffer(1, VK_SHADER_STAGE_COMPUTE_BIT, *m_DiffusePrecompute)
			.Allocate(m_Scope);

		uint32_t mips = m_SpecularLUT[i].Image->GetMipLevelsCount();
		for (uint32_t j = 0; j < mips; j++)
		{
			m_SpecularDescriptors[i * mips + j] = DescriptorSetDescriptor()
				.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_SpecularLUT[i].Views[j + 1]->GetImageView())
				.AddStorageBuffer(1, VK_SHADER_STAGE_COMPUTE_BIT, *m_SpecularPrecompute)
				.Allocate(m_Scope);

			m_CubemapMipDescriptors[i * mips + j] = DescriptorSetDescriptor()
				.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_CubemapLUT[i].Views[j]->GetImageView())
				.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_CubemapLUT[i].Views[j + 1]->GetImageView())
				.Allocate(m_Scope);
		}
	}

	m_CubemapPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_CubemapDescriptors[0]->GetLayout())
		.AddSpecializationConstant(0, Rg)
		.AddSpecializationConstant(1, Rt)
		.SetShaderName("cubemap_comp")
		.Construct(m_Scope);

	m_CubemapMipPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_CubemapMipDescriptors[0]->GetLayout())
		.SetShaderName("cubemap_mip_comp")
		.Construct(m_Scope);

	m_ConvolutionPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_ConvolutionDescriptors[0]->GetLayout())
		.AddDescriptorLayout(m_DiffuseDescriptors[0]->GetLayout())
		.SetShaderName("cube_convolution_comp")
		.AddSpecializationConstant(0, diffuseData.Count)
		.AddSpecializationConstant(1, diffuseData.ColorNorm)
		.Construct(m_Scope);

	VkPushConstantRange pushContants{};
	pushContants.size = sizeof(float);
	pushContants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	m_SpecularIBLPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_ConvolutionDescriptors[0]->GetLayout())
		.AddDescriptorLayout(m_SpecularDescriptors[0]->GetLayout())
		.SetShaderName("cube_convolution_spec_comp")
		.AddPushConstant(pushContants)
		.AddSpecializationConstant(0, Rg)
		.AddSpecializationConstant(1, Rt)
		.AddSpecializationConstant(2, specSamplesCount)
		.Construct(m_Scope);

	return 1;
}

VkBool32 VulkanBase::atmosphere_precompute()
{
	VulkanTexture DeltaE{};
	VulkanTexture DeltaSR{};
	VulkanTexture DeltaSM{};
	VulkanTexture DeltaJ{};

	std::unique_ptr<DescriptorSet> TrDSO;
	std::unique_ptr<DescriptorSet> DeltaEDSO;
	std::unique_ptr<DescriptorSet> DeltaSRSMDSO;
	std::unique_ptr<DescriptorSet> SingleScatterDSO;

	std::unique_ptr<DescriptorSet> DeltaJDSO;
	std::unique_ptr<DescriptorSet> DeltaEnDSO;
	std::unique_ptr<DescriptorSet> DeltaSDSO;
	std::unique_ptr<DescriptorSet> AddEDSO;
	std::unique_ptr<DescriptorSet> AddSDSO;

	std::unique_ptr<ComputePipeline> GenTrLUT;
	std::unique_ptr<ComputePipeline> GenDeltaELUT;
	std::unique_ptr<ComputePipeline> GenDeltaSRSMLUT;
	std::unique_ptr<ComputePipeline> GenSingleScatterLUT;
	std::unique_ptr<ComputePipeline> GenDeltaJLUT;
	std::unique_ptr<ComputePipeline> GenDeltaEnLUT;
	std::unique_ptr<ComputePipeline> GenDeltaSLUT;
	std::unique_ptr<ComputePipeline> AddE;
	std::unique_ptr<ComputePipeline> AddS;

	VkImageSubresourceRange subRes{};
	subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subRes.baseArrayLayer = 0;
	subRes.baseMipLevel = 0;
	subRes.layerCount = 1;
	subRes.levelCount = 1;

	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.arrayLayers = 1;
	imageCI.extent = { 256, 64, 1u };
	imageCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageCI.mipLevels = 1;
	imageCI.flags = 0;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCI.imageType = VK_IMAGE_TYPE_2D;

	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.subresourceRange = subRes;

	VmaAllocationCreateInfo imageAlloc{};
	imageAlloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkSampler ImageSampler = m_Scope.GetSampler(ESamplerType::PointClamp, 1);
	VkSampler ImageSampler2 = m_Scope.GetSampler(ESamplerType::BillinearClamp, 1);

	m_TransmittanceLUT.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	m_TransmittanceLUT.View = std::make_unique<VulkanImageView>(m_Scope, *m_TransmittanceLUT.Image);

	TrDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView())
		.Allocate(m_Scope);

	GenTrLUT = ComputePipelineDescriptor()
		.SetShaderName("transmittance_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(TrDSO->GetLayout())
		.Construct(m_Scope);

	imageCI.extent = { 64u, 16u, 1u };
	DeltaE.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	DeltaE.View = std::make_unique<VulkanImageView>(m_Scope, *DeltaE.Image);

	m_IrradianceLUT.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	m_IrradianceLUT.View = std::make_unique<VulkanImageView>(m_Scope, *m_IrradianceLUT.Image);

	DeltaEDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	GenDeltaELUT = ComputePipelineDescriptor()
		.SetShaderName("deltaE_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaEDSO->GetLayout())
		.Construct(m_Scope);

	imageCI.imageType = VK_IMAGE_TYPE_3D;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_3D;
	imageCI.extent = { 256u, 128u, 32u };
	DeltaSR.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	DeltaSR.View = std::make_unique<VulkanImageView>(m_Scope, *DeltaSR.Image);

	DeltaSM.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	DeltaSM.View = std::make_unique<VulkanImageView>(m_Scope, *DeltaSM.Image);

	DeltaSRSMDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView())
		.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSM.View->GetImageView())
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), ImageSampler2)
		.Allocate(m_Scope);

	GenDeltaSRSMLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaSRSM_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaSRSMDSO->GetLayout())
		.Construct(m_Scope);

	// imageCI.mipLevels = static_cast<uint32_t>(std::floor(std::log2(256u))) + 1;
	m_ScatteringLUT.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	m_ScatteringLUT.View = std::make_unique<VulkanImageView>(m_Scope, *m_ScatteringLUT.Image);
	// imageCI.mipLevels = 1;

	SingleScatterDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView(), ImageSampler2)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSM.View->GetImageView(), ImageSampler2)
		.Allocate(m_Scope);

	GenSingleScatterLUT = ComputePipelineDescriptor()
		.SetShaderName("singleScattering_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(SingleScatterDSO->GetLayout())
		.Construct(m_Scope);

	DeltaJ.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	DeltaJ.View = std::make_unique<VulkanImageView>(m_Scope, *DeltaJ.Image);

	DeltaJDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, DeltaJ.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), ImageSampler)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView(), ImageSampler)
		.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView(), ImageSampler)
		.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSM.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	GenDeltaJLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaJ_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaJDSO->GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int) })
		.Construct(m_Scope);

	DeltaEnDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, DeltaE.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView(), ImageSampler)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSM.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	GenDeltaEnLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaEn_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaEnDSO->GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int) })
		.Construct(m_Scope);

	DeltaSDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), ImageSampler)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, DeltaJ.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	GenDeltaSLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaS_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaSDSO->GetLayout())
		.Construct(m_Scope);

	AddEDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaE.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	AddE = ComputePipelineDescriptor()
		.SetShaderName("addE_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(AddEDSO->GetLayout())
		.Construct(m_Scope);

	AddSDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	AddS = ComputePipelineDescriptor()
		.SetShaderName("addS_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(AddSDSO->GetLayout())
		.Construct(m_Scope);

	VkCommandBuffer cmd;
	const Queue& Queue = m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT);

	Queue.AllocateCommandBuffers(1, &cmd);
	VkCommandBufferBeginInfo cmdBegin{};
	cmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	Queue.AllocateCommandBuffers2(1, &cmd);
	vkBeginCommandBuffer(cmd, &cmdBegin);

	GenTrLUT->BindPipeline(cmd);
	TrDSO->BindSet(0, cmd, *GenTrLUT);
	vkCmdDispatch(cmd, m_TransmittanceLUT.Image->GetExtent().width / 8u + uint32_t(m_TransmittanceLUT.Image->GetExtent().width % 8u > 0)
		, m_TransmittanceLUT.Image->GetExtent().height / 8u + uint32_t(m_TransmittanceLUT.Image->GetExtent().height % 8u > 0)
		, 1u);

	m_TransmittanceLUT.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

	GenDeltaELUT->BindPipeline(cmd);
	DeltaEDSO->BindSet(0, cmd, *GenDeltaELUT);
	vkCmdDispatch(cmd, m_IrradianceLUT.Image->GetExtent().width / 8u + uint32_t(m_IrradianceLUT.Image->GetExtent().width % 8u > 0),
		m_IrradianceLUT.Image->GetExtent().height / 8u + uint32_t(m_IrradianceLUT.Image->GetExtent().height % 8u > 0),
		1u);

	GenDeltaSRSMLUT->BindPipeline(cmd);
	DeltaSRSMDSO->BindSet(0, cmd, *GenDeltaSRSMLUT);
	vkCmdDispatch(cmd, DeltaSR.Image->GetExtent().width / 4u + uint32_t(DeltaSR.Image->GetExtent().width % 4u > 0),
		DeltaSR.Image->GetExtent().height / 4u + uint32_t(DeltaSR.Image->GetExtent().height % 4u > 0),
		DeltaSR.Image->GetExtent().depth / 4u + uint32_t(DeltaSR.Image->GetExtent().depth % 4u > 0));

	DeltaSM.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
	DeltaSR.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

	GenSingleScatterLUT->BindPipeline(cmd);
	SingleScatterDSO->BindSet(0, cmd, *GenSingleScatterLUT);
	vkCmdDispatch(cmd, m_ScatteringLUT.Image->GetExtent().width / 4u + uint32_t(m_ScatteringLUT.Image->GetExtent().width % 4u > 0),
		m_ScatteringLUT.Image->GetExtent().height / 4u + uint32_t(m_ScatteringLUT.Image->GetExtent().height % 4u > 0),
		m_ScatteringLUT.Image->GetExtent().depth / 4u + uint32_t(m_ScatteringLUT.Image->GetExtent().depth % 4u > 0));

	::EndCommandBuffer(cmd);
	Queue.Submit(cmd)
		.Wait();

	for (uint32_t Sample = 2; Sample <= 15; Sample++)
	{
		Queue.Wait();
		vkBeginCommandBuffer(cmd, &cmdBegin);

		DeltaJ.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		GenDeltaJLUT->PushConstants(cmd, &Sample, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		GenDeltaJLUT->BindPipeline(cmd);

		m_IrradianceLUT.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		DeltaJDSO->BindSet(0, cmd, *GenDeltaJLUT);
		vkCmdDispatch(cmd, DeltaJ.Image->GetExtent().width / 4u + uint32_t(DeltaJ.Image->GetExtent().width % 4u > 0),
			DeltaJ.Image->GetExtent().height / 4u + uint32_t(DeltaJ.Image->GetExtent().height % 4u > 0),
			DeltaJ.Image->GetExtent().depth / 4u + uint32_t(DeltaJ.Image->GetExtent().depth % 4u > 0));

		DeltaE.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		GenDeltaEnLUT->PushConstants(cmd, &Sample, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		GenDeltaEnLUT->BindPipeline(cmd);
		DeltaEnDSO->BindSet(0, cmd, *GenDeltaEnLUT);   
		vkCmdDispatch(cmd, DeltaE.Image->GetExtent().width / 8u + uint32_t(DeltaE.Image->GetExtent().width % 8u > 0),
			DeltaE.Image->GetExtent().height / 8u + uint32_t(DeltaE.Image->GetExtent().height % 8u > 0),
			1u);

		DeltaSR.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
		DeltaJ.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		GenDeltaSLUT->BindPipeline(cmd);
		DeltaSDSO->BindSet(0, cmd, *GenDeltaSLUT);
		vkCmdDispatch(cmd, DeltaSR.Image->GetExtent().width / 4u + uint32_t(DeltaSR.Image->GetExtent().width % 4u > 0),
			DeltaSR.Image->GetExtent().height / 4u + uint32_t(DeltaSR.Image->GetExtent().height % 4u > 0),
			DeltaSR.Image->GetExtent().depth / 4u + uint32_t(DeltaSR.Image->GetExtent().depth % 4u > 0));

		DeltaE.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
		m_IrradianceLUT.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		AddE->BindPipeline(cmd);
		AddEDSO->BindSet(0, cmd, *AddE);
		vkCmdDispatch(cmd, m_IrradianceLUT.Image->GetExtent().width / 8u + uint32_t(m_IrradianceLUT.Image->GetExtent().width % 8u > 0),
			m_IrradianceLUT.Image->GetExtent().height / 8u + uint32_t(m_IrradianceLUT.Image->GetExtent().height % 8u > 0),
			1u);

		DeltaSR.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		AddS->BindPipeline(cmd);
		AddSDSO->BindSet(0, cmd, *AddS);
		vkCmdDispatch(cmd, DeltaSR.Image->GetExtent().width / 4u + uint32_t(DeltaSR.Image->GetExtent().width % 4u > 0),
			DeltaSR.Image->GetExtent().height / 4u + uint32_t(DeltaSR.Image->GetExtent().height % 4u > 0),
			DeltaSR.Image->GetExtent().depth / 4u + uint32_t(DeltaSR.Image->GetExtent().depth % 4u > 0));

		vkEndCommandBuffer(cmd);
		Queue.Submit(cmd);
	}

	Queue.Wait()
		.FreeCommandBuffers(1, &cmd);

	m_TransmittanceLUT.Image->GenerateMipMaps();

	m_TransmittanceLUT.Image->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ScatteringLUT.Image->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_IrradianceLUT.Image->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	return 1;
}

VkBool32 VulkanBase::volumetric_precompute() 
{
	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkBufferCreateInfo cloudInfo{};
	cloudInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	cloudInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	cloudInfo.size = sizeof(CloudParameters);
	cloudInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	m_CloudLayer = std::make_unique<Buffer>(m_Scope, cloudInfo, allocCreateInfo);
	 
	SetCloudLayerSettings(CloudLayerProfile());

	m_VolumeShape.Image = GRNoise::GenerateCloudShapeNoise(m_Scope, { 128u, 128u, 128u }, 4u, 4u);
	m_VolumeShape.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeShape.Image);

	m_VolumeDetail.Image = GRNoise::GenerateCloudDetailNoise(m_Scope, { 64u, 64u, 64u }, 4u, 4u);
	m_VolumeDetail.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeDetail.Image);

	m_VolumeWeather.Image = GRNoise::GenerateWorleyPerlin(m_Scope, { 256u, 256u, 1u }, 16u, 8u, 8u);
	m_VolumeWeather.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeWeather.Image);

	m_VolumeWeatherCube.Image = GRNoise::GenerateWeatherCube(m_Scope, { 256u, 256u, 1u }, 16u, 8u, 8u);

	m_VolumeWeatherCube.Image->flags &= ~VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	m_VolumeWeatherCube.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeWeatherCube.Image);

	VkSampler SamplerClamp = m_Scope.GetSampler(ESamplerType::BillinearClamp, 1);

	// m_Volumetrics = std::make_unique<GraphicsObject>();
	m_VolumetricsDescriptor = DescriptorSetDescriptor()
		.AddUniformBuffer(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_CloudLayer)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_VolumeShape.View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, m_VolumeShape.View->GetSubresourceRange().levelCount))
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_VolumeDetail.View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, m_VolumeDetail.View->GetSubresourceRange().levelCount))
		.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, m_VolumeWeather.View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, m_VolumeWeather.View->GetSubresourceRange().levelCount))
		.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(5, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(6, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(7, VK_SHADER_STAGE_COMPUTE_BIT, m_VolumeWeatherCube.View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, m_VolumeWeatherCube.View->GetSubresourceRange().levelCount))
		.Allocate(m_Scope);

	VkDescriptorSetLayoutBinding bindngs[4];
	bindngs[0].binding = 0;
	bindngs[0].descriptorCount = 1;
	bindngs[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindngs[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindngs[0].pImmutableSamplers = VK_NULL_HANDLE;

	bindngs[1].binding = 1;
	bindngs[1].descriptorCount = 1;
	bindngs[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindngs[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindngs[1].pImmutableSamplers = VK_NULL_HANDLE;

	bindngs[2].binding = 2;
	bindngs[2].descriptorCount = 1;
	bindngs[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindngs[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindngs[2].pImmutableSamplers = VK_NULL_HANDLE;

	bindngs[3].binding = 3;
	bindngs[3].descriptorCount = 1;
	bindngs[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindngs[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindngs[3].pImmutableSamplers = VK_NULL_HANDLE;

	VkDescriptorSetLayoutCreateInfo dummyInfo{};
	dummyInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dummyInfo.bindingCount = 4;
	dummyInfo.pBindings = bindngs;

	VkDescriptorSetLayout dummy;
	vkCreateDescriptorSetLayout(m_Scope.GetDevice(), &dummyInfo, VK_NULL_HANDLE, &dummy);

	VkPushConstantRange ConstantOrder{};
	ConstantOrder.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	ConstantOrder.size = sizeof(int);

	ComputePipelineDescriptor VolumetricPSO{}; 
	VolumetricPSO.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(m_VolumetricsDescriptor->GetLayout())
		.AddDescriptorLayout(dummy)
		.AddPushConstant(ConstantOrder)
		.AddSpecializationConstant(0, Rg)
		.AddSpecializationConstant(1, Rt);

	m_VolumetricsAbovePipeline = VolumetricPSO.SetShaderName("volumetric_above_comp")
		.Construct(m_Scope);

	m_VolumetricsBetweenPipeline = VolumetricPSO.SetShaderName("volumetric_between_comp")
		.Construct(m_Scope);

	m_VolumetricsUnderPipeline = VolumetricPSO.SetShaderName("volumetric_under_comp")
		.Construct(m_Scope);

	m_VolumetricsComposePipeline = ComputePipelineDescriptor()
		.SetShaderName("volumetric_compose_comp")
		.AddDescriptorLayout(dummy)
		.AddPushConstant(ConstantOrder)
		.Construct(m_Scope);

	vkDestroyDescriptorSetLayout(m_Scope.GetDevice(), dummy, VK_NULL_HANDLE);

	return 1;
}

VkBool32 VulkanBase::terrain_init(const Buffer& VB, const GR::Shapes::GeoClipmap& shape)
{
	std::vector<uint32_t> queueFamilies = FindDeviceQueues(m_Scope.GetPhysicalDevice(), { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT });
	std::sort(queueFamilies.begin(), queueFamilies.end());
	queueFamilies.resize(std::distance(queueFamilies.begin(), std::unique(queueFamilies.begin(), queueFamilies.end())));

	VmaAllocationCreateInfo layerAllocCreateInfo{};
	layerAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkBufferCreateInfo terrainLayerInfo{};
	terrainLayerInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	terrainLayerInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	terrainLayerInfo.size = 100 * sizeof(TerrainLayerProfile) + sizeof(TerrainLayerProfile);
	terrainLayerInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	m_TerrainLayer = std::make_unique<Buffer>(m_Scope, terrainLayerInfo, layerAllocCreateInfo);

	int Layers = 1;
	float Scale = 1e1;
	TerrainLayerProfile defaultTerrain{};
	m_TerrainLayer->Update(&Layers, sizeof(int));
	m_TerrainLayer->Update(&Layers, Scale, sizeof(int));
	m_TerrainLayer->Update(&defaultTerrain, 100 * sizeof(TerrainLayerProfile), 16u);

	TerrainVBs.resize(m_ResourceCount);

	VkCommandBuffer cmd;
	m_Scope.GetQueue(VK_QUEUE_TRANSFER_BIT)
		.AllocateCommandBuffers(1, &cmd);

	::BeginOneTimeSubmitCmd(cmd);

	for (uint32_t i = 0; i < TerrainVBs.size(); i++)
	{
		VkBufferCreateInfo sbInfo{};
		sbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		sbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		sbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		sbInfo.queueFamilyIndexCount = queueFamilies.size();
		sbInfo.pQueueFamilyIndices = queueFamilies.data();
		sbInfo.size = VB.GetDescriptor().range;

		VmaAllocationCreateInfo sbAlloc{};
		sbAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		TerrainVBs[i] = std::make_unique<Buffer>(m_Scope, sbInfo, sbAlloc);

		VkBufferCopy region{};
		region.size = VB.GetDescriptor().range;
		vkCmdCopyBuffer(cmd, VB.GetBuffer(), TerrainVBs[i]->GetBuffer(), 1, &region);
	}

	::EndCommandBuffer(cmd);

	m_Scope.GetQueue(VK_QUEUE_TRANSFER_BIT)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	const uint32_t VertexCount = VB.GetDescriptor().range / sizeof(TerrainVertex);

	const uint32_t m = (glm::max(shape.m_VerPerRing, 7u) + 1) / 4;
	const uint32_t LUTExtent = static_cast<uint32_t>(2 * (m + 2) + 1);

	m_TerrainDispatches = VertexCount / 32 + static_cast<uint32_t>(VertexCount % 32 > 0);

	VkImageCreateInfo noiseInfo{};
	noiseInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	noiseInfo.arrayLayers = shape.m_Rings;
	noiseInfo.extent = { LUTExtent, LUTExtent, 1 };
	noiseInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	noiseInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	noiseInfo.mipLevels = 1;
	noiseInfo.flags = 0u;
	noiseInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	noiseInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	noiseInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	noiseInfo.queueFamilyIndexCount = queueFamilies.size();
	noiseInfo.pQueueFamilyIndices = queueFamilies.data();
	noiseInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	noiseInfo.imageType = VK_IMAGE_TYPE_2D;

	VkImageCreateInfo waterInfo{};
	waterInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	waterInfo.arrayLayers = shape.m_Rings;
	waterInfo.extent = { LUTExtent, LUTExtent, 1 };
	waterInfo.format = VK_FORMAT_R32_SFLOAT;
	waterInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	waterInfo.mipLevels = 1;
	waterInfo.flags = 0u;
	waterInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	waterInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	waterInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	waterInfo.queueFamilyIndexCount = queueFamilies.size();
	waterInfo.pQueueFamilyIndices = queueFamilies.data();
	waterInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	waterInfo.imageType = VK_IMAGE_TYPE_2D;

	VmaAllocationCreateInfo noiseAllocCreateInfo{};
	noiseAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	m_GrassSet.resize(m_ResourceCount);
	m_WaterSet.resize(m_ResourceCount);
	m_WaterLUT.resize(m_ResourceCount);
	m_TerrainLUT.resize(m_ResourceCount);
	m_TerrainSet.resize(m_ResourceCount);
	m_TerrainDrawSet.resize(m_ResourceCount);

	VkCommandBuffer clearCMD;
	VkClearColorValue Color;
	Color.float32[0] = 0.0;
	Color.float32[1] = 0.0;
	Color.float32[2] = 0.0;
	Color.float32[3] = 0.0;

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers2(1, &clearCMD);

	::BeginOneTimeSubmitCmd(clearCMD);
	for (uint32_t i = 0; i < m_ResourceCount; i++)
	{
		m_TerrainLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, noiseInfo, noiseAllocCreateInfo);
		m_TerrainLUT[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_TerrainLUT[i].Image);

		m_WaterLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, waterInfo, noiseAllocCreateInfo);
		m_WaterLUT[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_WaterLUT[i].Image);

		vkCmdClearColorImage(clearCMD, m_TerrainLUT[i].Image->GetImage(), VK_IMAGE_LAYOUT_GENERAL, &Color, 1, &m_TerrainLUT[i].Image->GetSubResourceRange());
		vkCmdClearColorImage(clearCMD, m_WaterLUT[i].Image->GetImage(), VK_IMAGE_LAYOUT_GENERAL, &Color, 1, &m_WaterLUT[i].Image->GetSubResourceRange());

		m_TerrainLUT[i].Image->TransitionLayout(clearCMD, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
		m_WaterLUT[i].Image->TransitionLayout(clearCMD, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
	}
	::EndCommandBuffer(clearCMD);

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.Submit(clearCMD)
		.Wait()
		.FreeCommandBuffers(1, &clearCMD);

	for (uint32_t i = 0; i < m_TerrainLUT.size(); i++)
	{
		m_TerrainSet[i] = DescriptorSetDescriptor()
			.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_TerrainLUT[i].View->GetImageView())
			.AddStorageBuffer(1, VK_SHADER_STAGE_COMPUTE_BIT, *m_TerrainLayer)
			.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_TerrainLUT[(i == 0 ? m_TerrainLUT.size() : i) - 1].View->GetImageView(), m_Scope.GetSampler(ESamplerType::PointClamp, 1))
			.AddStorageBuffer(3, VK_SHADER_STAGE_COMPUTE_BIT, *TerrainVBs[i])
			.AddStorageBuffer(4, VK_SHADER_STAGE_COMPUTE_BIT, *TerrainVBs[(i == 0 ? m_TerrainLUT.size() : i) - 1])
			// .AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_TerrainLUT[(i == 0 ? m_TerrainLUT.size() : i) - 1].View->GetImageView(), m_Scope.GetSampler(ESamplerType::PointClamp))
			// .AddUniformBuffer(3, VK_SHADER_STAGE_COMPUTE_BIT, *m_UBOBuffers[(i == 0 ? m_TerrainLUT.size() : i) - 1])
			.Allocate(m_Scope);

		m_WaterSet[i] = DescriptorSetDescriptor()
			.AddImageSampler(0, VK_SHADER_STAGE_COMPUTE_BIT, m_TerrainLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::PointClamp, 1))
			.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_WaterLUT[i].View->GetImageView())
			.Allocate(m_Scope);

		m_TerrainDrawSet[i] = DescriptorSetDescriptor()
			.AddImageSampler(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TerrainLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearMirror, 1))
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_WaterLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearClamp, 1))
			.Allocate(m_Scope);

		m_GrassSet[i] = DescriptorSetDescriptor()
			.AddImageSampler(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TerrainLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearClamp, 1))
			.AddStorageBuffer(1, VK_SHADER_STAGE_VERTEX_BIT, *TerrainVBs[i])
			// .AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_WaterLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearClamp))
			.Allocate(m_Scope);
	}

	{
		m_TerrainCompute = ComputePipelineDescriptor()
			.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
			.AddDescriptorLayout(m_TerrainSet[0]->GetLayout())
			.AddSpecializationConstant(0, Rg)
			.AddSpecializationConstant(1, Rt)
			.AddSpecializationConstant(2, VertexCount)
			.AddSpecializationConstant(3, shape.m_Scale)
			.AddSpecializationConstant(4, shape.m_MinHeight)
			.AddSpecializationConstant(5, shape.m_MaxHeight)
			.AddSpecializationConstant(6, shape.m_NoiseSeed)
			.AddSpecializationConstant(7, float(glm::ceil(float(shape.m_Rings) / 3.f) + 1.f))
			.AddSpecializationConstant(8, shape.m_Rings)
			.SetShaderName("terrain_noise_comp")
			.Construct(m_Scope);

		VkPushConstantRange ConstantCompose{};
		ConstantCompose.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		ConstantCompose.size = sizeof(int);

		m_TerrainCompose = ComputePipelineDescriptor()
			.AddDescriptorLayout(m_TerrainSet[0]->GetLayout())
			.AddSpecializationConstant(0, Rg)
			.AddSpecializationConstant(1, Rt)
			.AddSpecializationConstant(2, VertexCount)
			.AddSpecializationConstant(3, shape.m_Scale)
			.AddSpecializationConstant(4, shape.m_MinHeight)
			.AddSpecializationConstant(5, shape.m_MaxHeight)
			.AddSpecializationConstant(6, shape.m_NoiseSeed)
			.AddPushConstant(ConstantCompose)
			.SetShaderName("terrain_compose_comp")
			.Construct(m_Scope);

		m_WaterCompute = ComputePipelineDescriptor()
			.AddDescriptorLayout(m_WaterSet[0]->GetLayout())
			.AddSpecializationConstant(0, Rg)
			.AddSpecializationConstant(1, Rt)
			.AddSpecializationConstant(2, shape.m_Scale)
			.AddSpecializationConstant(3, shape.m_MinHeight)
			.AddSpecializationConstant(4, shape.m_MaxHeight)
			.AddSpecializationConstant(5, shape.m_NoiseSeed)
			.SetShaderName("erosion_comp")
			.Construct(m_Scope);

		std::unique_ptr<DescriptorSet> dummy = create_terrain_set(*m_DefaultWhite->View, *m_DefaultNormal->View, *m_DefaultARM->View);

		ConstantCompose.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		m_GrassPipeline = GraphicsPipelineDescriptor()
			.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
			.AddDescriptorLayout(m_GrassSet[0]->GetLayout())
			.AddDescriptorLayout(dummy->GetLayout())
			.AddPushConstant(ConstantCompose)
			.SetShaderStage("grass_vert", VK_SHADER_STAGE_VERTEX_BIT)
			.SetShaderStage("grass_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
			.SetCullMode(VK_CULL_MODE_NONE)
			.SetRenderPass(m_Scope.GetRenderPass(), 0)
			.SetBlendAttachments(3, nullptr)
			.AddSpecializationConstant(0, Rg, VK_SHADER_STAGE_VERTEX_BIT)
			.AddSpecializationConstant(1, Rt, VK_SHADER_STAGE_VERTEX_BIT)
			.AddSpecializationConstant(2, shape.m_Scale, VK_SHADER_STAGE_VERTEX_BIT)
			.AddSpecializationConstant(3, shape.m_MinHeight, VK_SHADER_STAGE_VERTEX_BIT)
			.AddSpecializationConstant(4, shape.m_MaxHeight, VK_SHADER_STAGE_VERTEX_BIT)
			.AddSpecializationConstant(5, float(glm::ceil(float(shape.m_Rings) / 3.f) + 1.f), VK_SHADER_STAGE_VERTEX_BIT)
			.Construct(m_Scope);
	}

	{
		VkDescriptorSetLayoutBinding bindngs[4];
		bindngs[0].binding = 0;
		bindngs[0].descriptorCount = 1;
		bindngs[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bindngs[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindngs[0].pImmutableSamplers = VK_NULL_HANDLE;

		bindngs[1].binding = 1;
		bindngs[1].descriptorCount = 1;
		bindngs[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindngs[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindngs[1].pImmutableSamplers = VK_NULL_HANDLE;

		bindngs[2].binding = 2;
		bindngs[2].descriptorCount = 1;
		bindngs[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindngs[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindngs[2].pImmutableSamplers = VK_NULL_HANDLE;

		bindngs[3].binding = 3;
		bindngs[3].descriptorCount = 1;
		bindngs[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindngs[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindngs[3].pImmutableSamplers = VK_NULL_HANDLE;

		VkDescriptorSetLayoutCreateInfo dummyInfo{};
		dummyInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dummyInfo.bindingCount = 4;
		dummyInfo.pBindings = bindngs;

		VkDescriptorSetLayout dummy;
		vkCreateDescriptorSetLayout(m_Scope.GetDevice(), &dummyInfo, VK_NULL_HANDLE, &dummy);

		VkPushConstantRange ConstantOrder{};
		ConstantOrder.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		ConstantOrder.size = sizeof(int);

		ComputePipelineDescriptor VolumetricPSO{};
		VolumetricPSO.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
			.AddDescriptorLayout(m_VolumetricsDescriptor->GetLayout())
			.AddDescriptorLayout(dummy)
			.AddPushConstant(ConstantOrder)
			.AddSpecializationConstant(0, float(Rg + shape.m_MinHeight))
			.AddSpecializationConstant(1, Rt);

		m_VolumetricsAbovePipeline = VolumetricPSO.SetShaderName("volumetric_above_comp")
			.Construct(m_Scope);

		m_VolumetricsBetweenPipeline = VolumetricPSO.SetShaderName("volumetric_between_comp")
			.Construct(m_Scope);

		m_VolumetricsUnderPipeline = VolumetricPSO.SetShaderName("volumetric_under_comp")
			.Construct(m_Scope);

		vkDestroyDescriptorSetLayout(m_Scope.GetDevice(), dummy, VK_NULL_HANDLE);
	}

	return 1;
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