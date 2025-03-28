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
	imageViewCI.viewType = (flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
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

	m_AsyncBuffers.resize(2 * m_SwapchainImages.size());
	m_PresentBuffers.resize(m_SwapchainImages.size());
	m_OwnershipBuffers.resize(2 * m_SwapchainImages.size());
	m_PresentSemaphores.resize(m_SwapchainImages.size());
	m_SwapchainSemaphores.resize(m_SwapchainImages.size());
	m_AsyncSemaphores.resize(2 * m_SwapchainImages.size());
	m_PresentFences.resize(m_SwapchainImages.size());
	m_AsyncFences.resize(2 * m_SwapchainImages.size());

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers(m_PresentBuffers.size(), m_PresentBuffers.data());

	m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT)
		.AllocateCommandBuffers2(m_SwapchainImages.size(), m_AsyncBuffers.data());

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers(m_SwapchainImages.size(), m_AsyncBuffers.data() + m_SwapchainImages.size());

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers2(m_SwapchainImages.size(), m_OwnershipBuffers.data());

	m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT)
		.AllocateCommandBuffers2(m_SwapchainImages.size(), m_OwnershipBuffers.data() + m_SwapchainImages.size());

	std::for_each(m_AsyncSemaphores.begin(), m_AsyncSemaphores.end(), [&, this](VkSemaphore& it) {
		res = CreateSemaphore(m_Scope.GetDevice(), &it) & res;
	});

	std::for_each(m_PresentSemaphores.begin(), m_PresentSemaphores.end(), [&, this](VkSemaphore& it) {
		res = CreateSemaphore(m_Scope.GetDevice(),  &it) & res;
	});

	std::for_each(m_SwapchainSemaphores.begin(), m_SwapchainSemaphores.end(), [&, this](VkSemaphore& it) {
		res = CreateSemaphore(m_Scope.GetDevice(), &it) & res;
	});

	std::for_each(m_PresentFences.begin(), m_PresentFences.end(), [&, this](VkFence& it) {
		res = CreateFence(m_Scope.GetDevice(), &it, VK_TRUE) & res;
	});


	std::for_each(m_AsyncFences.begin(), m_AsyncFences.end(), [&, this](VkFence& it) {
		res = CreateFence(m_Scope.GetDevice(), &it, VK_TRUE) & res;
	});

	res = CreateFence(m_Scope.GetDevice(), &m_AcquireFence, VK_FALSE) & res;

	res = prepare_renderer_resources() & res;
	res = atmosphere_precompute() & res;
	res = volumetric_precompute() & res;
	res = create_frame_pipelines()   & res;

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

	::CreateDescriptorPool(m_Scope.GetDevice(), pool_sizes.data(), pool_sizes.size(), 100, &m_ImguiPool);

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

	m_Camera.Transform.SetOffset({ 0.0, GR::Renderer::Rg, 0.0 });

	assert(res != 0);
}

VulkanBase::~VulkanBase() noexcept
{
	vkDeviceWaitIdle(m_Scope.GetDevice());

#ifdef INCLUDE_GUI
	ImGui_ImplVulkan_Shutdown();
	vkDestroyDescriptorPool(m_Scope.GetDevice(), m_ImguiPool, VK_NULL_HANDLE);
#endif

	m_TerrainCompute.reset();
	m_WaterCompute.reset();

	m_TerrainDrawSet.resize(0);
	m_TerrainSet.resize(0);
	m_TerrainLUT.resize(0);
	m_DiffuseIrradience.resize(0);
	m_SpecularLUT.resize(0);
	m_CubemapLUT.resize(0);
	m_BRDFLUT.resize(0);
	m_WaterSet.resize(0);
	m_WaterLUT.resize(0);

	m_Volumetrics.reset();
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

	vkDestroyFence(m_Scope.GetDevice(), m_AcquireFence, VK_NULL_HANDLE);

	std::erase_if(m_AsyncFences, [&, this](VkFence& it) {
		vkDestroyFence(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_PresentFences, [&, this](VkFence& it) {
		vkDestroyFence(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_AsyncSemaphores, [&, this](VkSemaphore& it) {
		vkDestroySemaphore(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_SwapchainSemaphores, [&, this](VkSemaphore& it) {
		vkDestroySemaphore(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_PresentSemaphores, [&, this](VkSemaphore& it) {
		vkDestroySemaphore(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersHR, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersLR, [&, this](VkFramebuffer& fb) {
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

	std::erase_if(m_FramebuffersCM, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersDC, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersSC, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersBRDF, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_SwapchainViews, [&, this](VkImageView& view) {
		vkDestroyImageView(m_Scope.GetDevice(), view, VK_NULL_HANDLE);
		return true;
	});

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.FreeCommandBuffers(m_PresentBuffers.size(), m_PresentBuffers.data());	

	m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT)
		.FreeCommandBuffers(m_SwapchainImages.size(), m_AsyncBuffers.data());

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.FreeCommandBuffers(m_SwapchainImages.size(), m_AsyncBuffers.data() + m_SwapchainImages.size());

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.FreeCommandBuffers(m_SwapchainImages.size(), m_OwnershipBuffers.data());

	m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT)
		.FreeCommandBuffers(m_SwapchainImages.size(), m_OwnershipBuffers.data() + m_SwapchainImages.size());

	m_BlurPipeline.reset();
	m_CubemapPipeline.reset();
	m_CompositionPipeline.reset();
	m_PostProcessPipeline.reset();
	m_ConvolutionPipeline.reset();
	m_SpecularIBLPipeline.reset();
	m_IntegrationPipeline.reset();

	m_DepthAttachmentsHR.resize(0);
	m_DepthViewsHR.resize(0);

	m_NormalAttachments.resize(0);
	m_NormalViews.resize(0);

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

	m_UBOSets.resize(0);

	m_Scope.Destroy();

	vkDestroySurfaceKHR(m_VkInstance, m_Surface, VK_NULL_HANDLE);
	vkDestroyInstance(m_VkInstance, VK_NULL_HANDLE);
}

bool VulkanBase::BeginFrame()
{
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0 || glfwWindowShouldClose(m_GlfwWindow))
		return false;

	assert(!m_InFrame, "Finish the frame in progress first!");

	uint32_t OldIndex = m_SwapchainIndex;

	std::vector<VkFence> Fences = { m_PresentFences[m_SwapchainIndex], m_AsyncFences[m_SwapchainImages.size() + m_SwapchainIndex], m_AsyncFences[m_SwapchainIndex] };
	vkWaitForFences(m_Scope.GetDevice(), Fences.size(), Fences.data(), VK_TRUE, UINT64_MAX);
	vkResetFences(m_Scope.GetDevice(), Fences.size(), Fences.data());

	vkAcquireNextImageKHR(m_Scope.GetDevice(), m_Scope.GetSwapchain(), UINT64_MAX, m_SwapchainSemaphores[m_SwapchainIndex], m_AcquireFence, &m_SwapchainIndex);

	vkWaitForFences(m_Scope.GetDevice(), 1, &m_AcquireFence, VK_TRUE, UINT64_MAX);
	vkResetFences(m_Scope.GetDevice(), 1, &m_AcquireFence);

	if (OldIndex != m_SwapchainIndex)
	{
		std::iter_swap(m_PresentFences.begin() + OldIndex, m_PresentFences.begin() + m_SwapchainIndex);
		std::iter_swap(m_PresentSemaphores.begin() + OldIndex, m_PresentSemaphores.begin() + m_SwapchainIndex);
		std::iter_swap(m_SwapchainSemaphores.begin() + OldIndex, m_SwapchainSemaphores.begin() + m_SwapchainIndex);
		std::iter_swap(m_PresentBuffers.begin() + OldIndex, m_PresentBuffers.begin() + m_SwapchainIndex);

		std::iter_swap(m_AsyncBuffers.begin() + OldIndex + m_SwapchainImages.size(), m_AsyncBuffers.begin() + m_SwapchainIndex + m_SwapchainImages.size());
		std::iter_swap(m_AsyncBuffers.begin() + OldIndex, m_AsyncBuffers.begin() + m_SwapchainIndex);

		std::iter_swap(m_AsyncFences.begin() + OldIndex + m_SwapchainImages.size(), m_AsyncFences.begin() + m_SwapchainIndex + m_SwapchainImages.size());
		std::iter_swap(m_AsyncFences.begin() + OldIndex, m_AsyncFences.begin() + m_SwapchainIndex);

		std::iter_swap(m_AsyncSemaphores.begin() + OldIndex + m_SwapchainImages.size(), m_AsyncSemaphores.begin() + m_SwapchainIndex + m_SwapchainImages.size());
		std::iter_swap(m_AsyncSemaphores.begin() + OldIndex, m_AsyncSemaphores.begin() + m_SwapchainIndex);
	}

	// Udpate UBO
	{
		glm::dmat4 view_matrix = m_Camera.get_view_matrix();
		glm::dmat4 view_matrix_inverse = glm::inverse(view_matrix);
		glm::mat4 projection_matrix = m_Camera.get_projection_matrix();
		glm::mat4 projection_matrix_inverse = glm::inverse(projection_matrix);
		glm::dmat4 view_proj_matrix = static_cast<glm::dmat4>(projection_matrix) * view_matrix;
		glm::dvec4 CameraPositionFP64 = glm::dvec4(m_Camera.Transform.GetOffset(), 1.0);
		glm::vec4 CameraPosition = glm::vec4(m_Camera.Transform.GetOffset(), 1.0);
		glm::vec4 Sun = glm::vec4(glm::normalize(m_SunDirection), 0.0);
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
			CameraPositionFP64,
			projection_matrix,
			projection_matrix_inverse,
			CameraPosition,
			Sun,
			CameraUp,
			CameraRight,
			CameraForward,
			ScreenSize,
			CameraRadius,
			Time,
		};

		m_UBOBuffers[m_SwapchainIndex]->Update(static_cast<void*>(&Uniform), sizeof(Uniform));
	}

	VkCommandBuffer GraphicsCmd = m_AsyncBuffers[m_SwapchainImages.size() + m_SwapchainIndex];
	VkCommandBuffer ComputeCmd = m_AsyncBuffers[m_SwapchainIndex];

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(GraphicsCmd, &beginInfo);
	vkBeginCommandBuffer(ComputeCmd, &beginInfo);

	// Prepare render targets
	{
		std::array<VkImageMemoryBarrier, 6> barriers = {};
		barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[0].image = m_HdrAttachmentsHR[m_SwapchainIndex]->GetImage();
		barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[0].subresourceRange.baseArrayLayer = 0;
		barriers[0].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barriers[0].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barriers[0].subresourceRange.baseMipLevel = 0;
		barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barriers[0].srcAccessMask = 0;
		barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[1].image = m_HdrAttachmentsLR[m_SwapchainIndex]->GetImage();
		barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[1].subresourceRange.baseArrayLayer = 0;
		barriers[1].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barriers[1].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barriers[1].subresourceRange.baseMipLevel = 0;
		barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barriers[1].srcAccessMask = 0;
		barriers[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		barriers[2].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[2].image = m_DepthAttachmentsHR[m_SwapchainIndex]->GetImage();
		barriers[2].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[2].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[2].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		barriers[2].subresourceRange.baseArrayLayer = 0;
		barriers[2].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barriers[2].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barriers[2].subresourceRange.baseMipLevel = 0;
		barriers[2].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[2].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		barriers[2].srcAccessMask = 0;
		barriers[2].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		 
		barriers[3].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[3].image = m_DepthAttachmentsLR[m_SwapchainIndex]->GetImage();
		barriers[3].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[3].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	
		barriers[3].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		barriers[3].subresourceRange.baseArrayLayer = 0;
		barriers[3].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barriers[3].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barriers[3].subresourceRange.baseMipLevel = 0;
		barriers[3].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[3].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		barriers[3].srcAccessMask = 0;
		barriers[3].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		barriers[4].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[4].image = m_BlurAttachments[2 * m_SwapchainIndex]->GetImage();
		barriers[4].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[4].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[4].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[4].subresourceRange.baseArrayLayer = 0;
		barriers[4].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barriers[4].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barriers[4].subresourceRange.baseMipLevel = 0;
		barriers[4].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[4].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[4].srcAccessMask = 0;
		barriers[4].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

		barriers[5].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[5].image = m_BlurAttachments[2 * m_SwapchainIndex + 1]->GetImage();
		barriers[5].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[5].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[5].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[5].subresourceRange.baseArrayLayer = 0;
		barriers[5].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barriers[5].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barriers[5].subresourceRange.baseMipLevel = 0;
		barriers[5].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[5].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[5].srcAccessMask = 0;
		barriers[5].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

		vkCmdPipelineBarrier(GraphicsCmd, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, barriers.size(), barriers.data());
	}

	// Draw Low Resolution Background
	{
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].depthStencil = { 0.0f, 0 };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = m_FramebuffersLR[m_SwapchainIndex];
		renderPassInfo.renderPass = m_Scope.GetLowResRenderPass();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { m_Scope.GetSwapchainExtent().width / LRr, m_Scope.GetSwapchainExtent().height / LRr };
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues;

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(m_Scope.GetSwapchainExtent().width / LRr);
		viewport.height = static_cast<float>(m_Scope.GetSwapchainExtent().height / LRr);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(GraphicsCmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { m_Scope.GetSwapchainExtent().width / LRr, m_Scope.GetSwapchainExtent().height / LRr };
		vkCmdSetScissor(GraphicsCmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(GraphicsCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_UBOSets[m_SwapchainIndex]->BindSet(0, GraphicsCmd, *m_Volumetrics->pipeline);
		m_Volumetrics->descriptorSet->BindSet(1, GraphicsCmd, *m_Volumetrics->pipeline);
		m_Volumetrics->pipeline->BindPipeline(GraphicsCmd);
		vkCmdDraw(GraphicsCmd, 3, 1, 0, 0);

		vkCmdEndRenderPass(GraphicsCmd);
	}

	{
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = m_FramebuffersCM[m_SwapchainIndex];
		renderPassInfo.renderPass = m_Scope.GetCubemapPass();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { CubeR, CubeR };

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(CubeR);
		viewport.height = static_cast<float>(CubeR);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(GraphicsCmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { CubeR, CubeR };
		vkCmdSetScissor(GraphicsCmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(GraphicsCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_CubemapPipeline->BindPipeline(GraphicsCmd);
		m_CubemapDescriptors[m_SwapchainIndex]->BindSet(0, GraphicsCmd, *m_CubemapPipeline);
		vkCmdDraw(GraphicsCmd, 36, 1, 0, 0);

		vkCmdEndRenderPass(GraphicsCmd);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = m_CubemapLUT[m_SwapchainIndex].Image->GetImage();
		barrier.subresourceRange = m_CubemapLUT[m_SwapchainIndex].Image->GetSubResourceRange();
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		vkCmdPipelineBarrier(GraphicsCmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

		renderPassInfo.framebuffer = m_FramebuffersDC[m_SwapchainIndex];
		vkCmdBeginRenderPass(GraphicsCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_ConvolutionPipeline->BindPipeline(GraphicsCmd);
		m_ConvolutionDescriptors[m_SwapchainIndex]->BindSet(0, GraphicsCmd, *m_ConvolutionPipeline);
		vkCmdDraw(GraphicsCmd, 36, 1, 0, 0);

		vkCmdEndRenderPass(GraphicsCmd);

		uint32_t mips = m_SpecularLUT[m_SwapchainIndex].Image->GetMipLevelsCount();
		for (uint32_t mip = 0; mip < mips; mip++)
		{
			uint32_t ScaledR = CubeR >> mip;

			viewport.width = static_cast<float>(ScaledR);
			viewport.height = static_cast<float>(ScaledR);
			vkCmdSetViewport(GraphicsCmd, 0, 1, &viewport);

			scissor.extent = { ScaledR, ScaledR };
			vkCmdSetScissor(GraphicsCmd, 0, 1, &scissor);

			renderPassInfo.renderArea.extent = { ScaledR, ScaledR };
			renderPassInfo.framebuffer = m_FramebuffersSC[m_SwapchainIndex * mips + mip];
			vkCmdBeginRenderPass(GraphicsCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			m_SpecularIBLPipeline->BindPipeline(GraphicsCmd);
			m_ConvolutionDescriptors[m_SwapchainIndex]->BindSet(0, GraphicsCmd, *m_SpecularIBLPipeline);

			float pushConstant = float(mip) / float(mips - 1.0);
			m_SpecularIBLPipeline->PushConstants(GraphicsCmd, &pushConstant, sizeof(float), 0, VK_SHADER_STAGE_FRAGMENT_BIT);
			vkCmdDraw(GraphicsCmd, 36, 1, 0, 0);

			vkCmdEndRenderPass(GraphicsCmd);
		}

		viewport.width = static_cast<float>(m_BRDFLUT[m_SwapchainIndex].Image->GetExtent().width);
		viewport.height = static_cast<float>(m_BRDFLUT[m_SwapchainIndex].Image->GetExtent().height);
		vkCmdSetViewport(GraphicsCmd, 0, 1, &viewport);

		scissor.extent = { m_BRDFLUT[m_SwapchainIndex].Image->GetExtent().width, m_BRDFLUT[m_SwapchainIndex].Image->GetExtent().height };
		vkCmdSetScissor(GraphicsCmd, 0, 1, &scissor);

		renderPassInfo.renderArea.extent = { m_BRDFLUT[m_SwapchainIndex].Image->GetExtent().width, m_BRDFLUT[m_SwapchainIndex].Image->GetExtent().height };
		renderPassInfo.framebuffer = m_FramebuffersBRDF[m_SwapchainIndex];
		vkCmdBeginRenderPass(GraphicsCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_IntegrationPipeline->BindPipeline(GraphicsCmd);
		vkCmdDraw(GraphicsCmd, 3, 1, 0, 0);

		vkCmdEndRenderPass(GraphicsCmd);
	}

	vkEndCommandBuffer(GraphicsCmd);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &GraphicsCmd;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_AsyncSemaphores[m_SwapchainImages.size() + m_SwapchainIndex];

	vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, m_AsyncFences[m_SwapchainImages.size() + m_SwapchainIndex]);

	GraphicsCmd = m_PresentBuffers[m_SwapchainIndex];
	vkBeginCommandBuffer(GraphicsCmd, &beginInfo);

	// Start async compute to update terrain height
	if (m_TerrainCompute.get())
	{
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = m_TerrainLUT[m_SwapchainIndex].Image->GetImage();
			barrier.subresourceRange = m_TerrainLUT[m_SwapchainIndex].Image->GetSubResourceRange();
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(ComputeCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

			barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barrier.image = m_WaterLUT[m_SwapchainIndex].Image->GetImage();
			barrier.subresourceRange = m_WaterLUT[m_SwapchainIndex].Image->GetSubResourceRange();
			vkCmdPipelineBarrier(ComputeCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
		}

		m_UBOSets[m_SwapchainIndex]->BindSet(0, ComputeCmd, *m_TerrainCompute);
		m_TerrainSet[m_SwapchainIndex]->BindSet(1, ComputeCmd, *m_TerrainCompute);
		m_TerrainCompute->BindPipeline(ComputeCmd);
		vkCmdDispatch(ComputeCmd, m_TerrainDispatches, 1, 1);

		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = m_TerrainLUT[m_SwapchainIndex].Image->GetImage();
			barrier.subresourceRange = m_TerrainLUT[m_SwapchainIndex].Image->GetSubResourceRange();
			barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(ComputeCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
		}

#if 1
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = m_WaterLUT[m_SwapchainIndex].Image->GetImage();
			barrier.subresourceRange = m_WaterLUT[m_SwapchainIndex].Image->GetSubResourceRange();
			barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

			VkClearColorValue Color;
			Color.float32[0] = 0.0;
			vkCmdClearColorImage(ComputeCmd, m_WaterLUT[m_SwapchainIndex].Image->GetImage(), VK_IMAGE_LAYOUT_GENERAL, &Color, 1, &m_WaterLUT[m_SwapchainIndex].Image->GetSubResourceRange());

			m_UBOSets[m_SwapchainIndex]->BindSet(0, ComputeCmd, *m_WaterCompute);
			m_WaterSet[m_SwapchainIndex]->BindSet(1, ComputeCmd, *m_WaterCompute);
			m_WaterCompute->BindPipeline(ComputeCmd);
			vkCmdDispatch(ComputeCmd, m_WaterLUT[0].Image->GetExtent().width / 32 + 1, m_WaterLUT[0].Image->GetExtent().height / 32 + 1, m_WaterLUT[0].Image->GetArrayLayers());
			vkCmdPipelineBarrier(ComputeCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
		}
#endif

		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcQueueFamilyIndex = m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex();
			barrier.dstQueueFamilyIndex = m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex();
			barrier.image = m_TerrainLUT[m_SwapchainIndex].Image->GetImage();
			barrier.subresourceRange = m_TerrainLUT[m_SwapchainIndex].Image->GetSubResourceRange();
			barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

			vkCmdPipelineBarrier(ComputeCmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
			vkCmdPipelineBarrier(GraphicsCmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

			barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcQueueFamilyIndex = m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex();
			barrier.dstQueueFamilyIndex = m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex();
			barrier.image = m_WaterLUT[m_SwapchainIndex].Image->GetImage();
			barrier.subresourceRange = m_WaterLUT[m_SwapchainIndex].Image->GetSubResourceRange();

			vkCmdPipelineBarrier(ComputeCmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
			vkCmdPipelineBarrier(GraphicsCmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
		}

		m_TerrainLUT[m_SwapchainIndex].Image->GenerateMipMaps(GraphicsCmd);
		m_WaterLUT[m_SwapchainIndex].Image->GenerateMipMaps(GraphicsCmd);
	}

	{
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
		renderPassInfo.framebuffer = m_FramebuffersHR[m_SwapchainIndex];
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
		vkCmdSetViewport(GraphicsCmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Scope.GetSwapchainExtent();
		vkCmdSetScissor(GraphicsCmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(GraphicsCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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

	VkCommandBuffer GraphicsCmd = m_PresentBuffers[m_SwapchainIndex];
	VkCommandBuffer ComputeCmd = m_AsyncBuffers[m_SwapchainIndex];

	// Draw High Resolution Objects
	{
		vkCmdEndRenderPass(GraphicsCmd);
	}

	// Process the scene data
	{
		std::array<VkClearValue, 1> clearValues;
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = m_FramebuffersCP[m_SwapchainIndex];
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
		vkCmdSetViewport(GraphicsCmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Scope.GetSwapchainExtent();
		vkCmdSetScissor(GraphicsCmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(GraphicsCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_CompositionDescriptors[m_SwapchainIndex]->BindSet(0, GraphicsCmd, *m_CompositionPipeline);
		m_CompositionPipeline->BindPipeline(GraphicsCmd);
		vkCmdDraw(GraphicsCmd, 3, 1, 0, 0);

		vkCmdEndRenderPass(GraphicsCmd);
	}

	{
		std::array<VkImageMemoryBarrier, 2> barrier{};
		barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier[0].image = m_BlurAttachments[2 * m_SwapchainIndex]->GetImage();
		barrier[0].subresourceRange = m_BlurAttachments[2 * m_SwapchainIndex]->GetSubResourceRange();
		barrier[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		barrier[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier[1].image = m_BlurAttachments[2 * m_SwapchainIndex + 1]->GetImage();
		barrier[1].subresourceRange = m_BlurAttachments[2 * m_SwapchainIndex + 1]->GetSubResourceRange();
		barrier[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		const int Radius = 10;
		m_BlurPipeline->BindPipeline(GraphicsCmd);
		m_BlurDescriptors[m_SwapchainIndex]->BindSet(0, GraphicsCmd, *m_BlurPipeline);
		for (int i = 0; i <= Radius; i++)
		{
			float Order = float(i);
			m_BlurPipeline->PushConstants(GraphicsCmd, &Order, sizeof(float), 0, VK_SHADER_STAGE_COMPUTE_BIT);
			vkCmdDispatch(GraphicsCmd, m_Scope.GetSwapchainExtent().width / 32 + static_cast<uint32_t>(m_Scope.GetSwapchainExtent().width % 32 > 0),
				m_Scope.GetSwapchainExtent().height / 32 + static_cast<uint32_t>(m_Scope.GetSwapchainExtent().height % 32 > 0), 1);

			if (i == Radius)
			{
				barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			}

			vkCmdPipelineBarrier(GraphicsCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, barrier.size(), barrier.data());
		}
	}

	{
		std::array<VkClearValue, 1> clearValues;
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = m_FramebuffersPP[m_SwapchainIndex];
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
		vkCmdSetViewport(GraphicsCmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Scope.GetSwapchainExtent();
		vkCmdSetScissor(GraphicsCmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(GraphicsCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_PostProcessDescriptors[m_SwapchainIndex]->BindSet(0, GraphicsCmd, *m_PostProcessPipeline);
		m_PostProcessPipeline->BindPipeline(GraphicsCmd);
		vkCmdDraw(GraphicsCmd, 3, 1, 0, 0);

		vkCmdNextSubpass(GraphicsCmd, VK_SUBPASS_CONTENTS_INLINE);

#ifdef INCLUDE_GUI
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), GraphicsCmd);
#endif

		vkCmdEndRenderPass(GraphicsCmd);
	}

	vkEndCommandBuffer(ComputeCmd);
	vkEndCommandBuffer(GraphicsCmd);

	VkSubmitInfo submitInfo{};
	std::vector<VkSemaphore> waitSemaphores = { m_SwapchainSemaphores[m_SwapchainIndex], m_AsyncSemaphores[m_SwapchainImages.size() + m_SwapchainIndex], m_AsyncSemaphores[m_SwapchainIndex] };
	std::vector<VkSemaphore> signalSemaphores = { m_PresentSemaphores[m_SwapchainIndex] };

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = waitSemaphores.size();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_PresentBuffers[m_SwapchainIndex];
	submitInfo.signalSemaphoreCount = signalSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT)
		.Submit(ComputeCmd, m_AsyncFences[m_SwapchainIndex], m_AsyncSemaphores[m_SwapchainIndex]);

	VkResult res = vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, m_PresentFences[m_SwapchainIndex]);

	if (m_TerrainCompute.get())
	{
		transfer_ownership(VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, m_TerrainLUT[m_SwapchainIndex].Image.get(), m_AsyncFences[m_SwapchainIndex], m_AsyncSemaphores[m_SwapchainIndex]);
		transfer_ownership(VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, m_WaterLUT[m_SwapchainIndex].Image.get(), m_AsyncFences[m_SwapchainIndex], m_AsyncSemaphores[m_SwapchainIndex]);
	}

	assert(res != VK_ERROR_DEVICE_LOST);

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = signalSemaphores.size();
	presentInfo.pWaitSemaphores = signalSemaphores.data();
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_Scope.GetSwapchain();
	presentInfo.pImageIndices = &m_SwapchainIndex;
	presentInfo.pResults = VK_NULL_HANDLE;
	
	vkQueuePresentKHR(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), &presentInfo);

	m_SwapchainIndex = (m_SwapchainIndex + 1) % m_SwapchainImages.size();

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

	std::erase_if(m_FramebuffersLR, [&, this](VkFramebuffer& fb) {
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

	std::erase_if(m_FramebuffersCM, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersDC, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});

	std::erase_if(m_FramebuffersSC, [&, this](VkFramebuffer& fb) {
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

	m_SwapchainImages.resize(0);

	m_Scope.RecreateSwapchain(m_Surface);

	if (m_Scope.GetSwapchain() == VK_NULL_HANDLE)
		return;

	create_swapchain_images();
	create_framebuffers();
	create_frame_pipelines();

	m_Camera.Projection.SetAspect(static_cast<float>(m_Scope.GetSwapchainExtent().width) / static_cast<float>(m_Scope.GetSwapchainExtent().height));

	//it shouldn't change afaik, but better to keep it under control
	assert(m_SwapchainImages.size() == m_PresentBuffers.size());
}

void VulkanBase::Wait() const
{
	vkWaitForFences(m_Scope.GetDevice(), m_PresentFences.size(), m_PresentFences.data(), VK_TRUE, UINT64_MAX);
	vkWaitForFences(m_Scope.GetDevice(), m_AsyncFences.size(), m_AsyncFences.data(), VK_TRUE, UINT64_MAX);
	// vkResetFences(m_Scope.GetDevice(), Fences.size(), Fences.data());
}

void VulkanBase::SetCloudLayerSettings(CloudLayerProfile settings)
{
	m_CloudLayer->Update(&settings, sizeof(CloudLayerProfile));
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

	m_SwapchainImages.resize(imagesCount);
	m_SwapchainViews.resize(imagesCount);

	m_DepthAttachmentsHR.resize(imagesCount);
	m_DepthViewsHR.resize(imagesCount);

	m_HdrAttachmentsHR.resize(imagesCount);
	m_HdrViewsHR.resize(imagesCount);

	m_DeferredAttachments.resize(imagesCount);
	m_DeferredViews.resize(imagesCount);

	m_BlurAttachments.resize(2 * imagesCount);
	m_BlurViews.resize(2 * imagesCount);

	m_NormalAttachments.resize(imagesCount);
	m_NormalViews.resize(imagesCount);

	m_HdrAttachmentsLR.resize(imagesCount);
	m_HdrViewsLR.resize(imagesCount);

	m_DepthAttachmentsLR.resize(imagesCount);
	m_DepthViewsLR.resize(imagesCount);

	m_DiffuseIrradience.resize(imagesCount);
	m_SpecularLUT.resize(imagesCount);
	m_CubemapLUT.resize(imagesCount);
	m_BRDFLUT.resize(imagesCount);

	VkBool32 res = vkGetSwapchainImagesKHR(m_Scope.GetDevice(), m_Scope.GetSwapchain(), &imagesCount, m_SwapchainImages.data()) == VK_SUCCESS;

	std::vector<uint32_t> queueFamilies = { m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex() };
	std::sort(queueFamilies.begin(), queueFamilies.end());
	queueFamilies.resize(std::distance(queueFamilies.begin(), std::unique(queueFamilies.begin(), queueFamilies.end())));

	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	for (size_t i = 0; i < m_SwapchainImages.size(); i++)
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
		hdrInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
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
		depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		depthInfo.queueFamilyIndexCount = queueFamilies.size();
		depthInfo.pQueueFamilyIndices = queueFamilies.data();

		res = (vkCreateImageView(m_Scope.GetDevice(), &viewInfo, VK_NULL_HANDLE, &m_SwapchainViews[i]) == VK_SUCCESS) & res;

		m_HdrAttachmentsHR[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_HdrViewsHR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_HdrAttachmentsHR[i]);

		hdrInfo.usage &= ~VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		hdrInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

		m_BlurAttachments[2 * i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_BlurViews[2 * i] = std::make_unique<VulkanImageView>(m_Scope, *m_BlurAttachments[2 * i]);

		m_BlurAttachments[2 * i + 1] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_BlurViews[2 * i + 1] = std::make_unique<VulkanImageView>(m_Scope, *m_BlurAttachments[2 * i + 1]);

		hdrInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
		hdrInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		m_DeferredAttachments[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_DeferredViews[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DeferredAttachments[i]);

		hdrInfo.format = VK_FORMAT_R8G8B8A8_SNORM;
		m_NormalAttachments[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_NormalViews[i] = std::make_unique<VulkanImageView>(m_Scope, *m_NormalAttachments[i]);

		m_DepthAttachmentsHR[i] = std::make_unique<VulkanImage>(m_Scope, depthInfo, allocCreateInfo);
		m_DepthViewsHR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DepthAttachmentsHR[i]);

		hdrInfo.extent.width /= LRr;
		hdrInfo.extent.height /= LRr;
		hdrInfo.usage &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		hdrInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		hdrInfo.format = m_Scope.GetHDRFormat();
		m_HdrAttachmentsLR[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_HdrViewsLR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_HdrAttachmentsLR[i]);

		hdrInfo.extent.width = 512;
		hdrInfo.extent.height = 512;
		m_BRDFLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_BRDFLUT[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_BRDFLUT[i].Image);

		hdrInfo.extent.width = CubeR;
		hdrInfo.extent.height = CubeR;
		hdrInfo.arrayLayers = 6;
		hdrInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		m_CubemapLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_CubemapLUT[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_CubemapLUT[i].Image);

		m_DiffuseIrradience[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_DiffuseIrradience[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_DiffuseIrradience[i].Image);

		hdrInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(CubeR))) + 1;
		hdrInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		m_SpecularLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);

		m_SpecularLUT[i].Views.reserve(hdrInfo.mipLevels + 1);
		VkImageSubresourceRange subRes = m_SpecularLUT[i].Image->GetSubResourceRange();
		subRes.baseMipLevel = 0;
		subRes.levelCount = hdrInfo.mipLevels;
		m_SpecularLUT[i].Views.emplace_back(std::make_unique<VulkanImageView>(m_Scope, *m_SpecularLUT[i].Image, subRes));
		for (uint32_t j = 0; j < hdrInfo.mipLevels; j++)
		{
			subRes.baseMipLevel = j;
			subRes.levelCount = 1u;
			m_SpecularLUT[i].Views.emplace_back(std::make_unique<VulkanImageView>(m_Scope, *m_SpecularLUT[i].Image, subRes));
		}

		int u = m_SpecularLUT[i].Views.size();

		depthInfo.extent.width /= LRr;
		depthInfo.extent.height /= LRr;
		depthInfo.usage &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		depthInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		m_DepthAttachmentsLR[i] = std::make_unique<VulkanImage>(m_Scope, depthInfo, allocCreateInfo);
		m_DepthViewsLR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DepthAttachmentsLR[i]);
	}

	return res;
}

VkBool32 VulkanBase::create_framebuffers()
{
	assert(m_SwapchainImages.size() > 0 && m_Scope.GetRenderPass() != VK_NULL_HANDLE);

	m_FramebuffersHR.resize(m_SwapchainImages.size());
	m_FramebuffersLR.resize(m_SwapchainImages.size());
	m_FramebuffersCP.resize(m_SwapchainImages.size());
	m_FramebuffersPP.resize(m_SwapchainImages.size());
	m_FramebuffersCM.resize(m_SwapchainImages.size());
	m_FramebuffersDC.resize(m_SwapchainImages.size());
	m_FramebuffersBRDF.resize(m_SwapchainImages.size());
	m_FramebuffersSC.resize(m_SpecularLUT[0].Image->GetMipLevelsCount() * m_SwapchainImages.size());

	VkBool32 res = 1;
	for (size_t i = 0; i < m_SwapchainImages.size(); i++)
	{
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetRenderPass(), { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 }, { m_HdrViewsHR[i]->GetImageView(), m_NormalViews[i]->GetImageView(),m_DeferredViews[i]->GetImageView(), m_DepthViewsHR[i]->GetImageView() }, &m_FramebuffersHR[i]) & res;
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetLowResRenderPass(), { m_Scope.GetSwapchainExtent().width / LRr, m_Scope.GetSwapchainExtent().height / LRr, 1 }, { m_HdrViewsLR[i]->GetImageView(), m_DepthViewsLR[i]->GetImageView()}, &m_FramebuffersLR[i]) & res;
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetCompositionPass(), { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 }, { m_HdrViewsHR[i]->GetImageView() }, &m_FramebuffersCP[i]);
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetPostProcessPass(), { m_Scope.GetSwapchainExtent().width, m_Scope.GetSwapchainExtent().height, 1 }, { m_SwapchainViews[i] }, &m_FramebuffersPP[i]);
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetCubemapPass(), { CubeR, CubeR, 6 }, { m_CubemapLUT[i].View->GetImageView() }, &m_FramebuffersCM[i]);
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetCubemapPass(), { CubeR, CubeR, 6 }, { m_DiffuseIrradience[i].View->GetImageView() }, &m_FramebuffersDC[i]);
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetCubemapPass(), { m_BRDFLUT[i].Image->GetExtent().width, m_BRDFLUT[i].Image->GetExtent().height, 1}, {m_BRDFLUT[i].View->GetImageView()}, &m_FramebuffersBRDF[i]);

		for (uint32_t j = 0; j < m_SpecularLUT[i].Image->GetMipLevelsCount(); j++)
			res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetCubemapPass(), { CubeR >> j, CubeR >> j, 6 }, { m_SpecularLUT[i].Views[j + 1]->GetImageView()}, &m_FramebuffersSC[m_SpecularLUT[0].Image->GetMipLevelsCount() * i + j]);
	}

	return res;
}

VkBool32 VulkanBase::create_frame_pipelines()
{
	VkBool32 res = 1;

	m_CompositionDescriptors.resize(m_SwapchainImages.size());
	m_PostProcessDescriptors.resize(m_SwapchainImages.size());
	m_ConvolutionDescriptors.resize(m_SwapchainImages.size());
	m_CubemapDescriptors.resize(m_SwapchainImages.size());
	m_BlurDescriptors.resize(m_SwapchainImages.size());

	VkSampler SamplerPoint = m_Scope.GetSampler(ESamplerType::PointClamp);
	VkSampler SamplerLinear = m_Scope.GetSampler(ESamplerType::LinearClamp);
	VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::LinearRepeat);

	VkPushConstantRange BlurConst{};
	BlurConst.size = sizeof(float);
	BlurConst.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	for (size_t i = 0; i < m_SwapchainImages.size(); i++)
	{
		m_CompositionDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsHR[i]->GetImageView(), SamplerPoint, VK_IMAGE_LAYOUT_GENERAL)
			.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, m_NormalViews[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, m_DeferredViews[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(4, VK_SHADER_STAGE_FRAGMENT_BIT, m_DepthViewsHR[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(5, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsLR[i]->GetImageView(), SamplerLinear)
			.AddImageSampler(6, VK_SHADER_STAGE_FRAGMENT_BIT, m_DepthViewsLR[i]->GetImageView(), SamplerLinear)
			.AddImageSampler(7, VK_SHADER_STAGE_FRAGMENT_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(8, VK_SHADER_STAGE_FRAGMENT_BIT, m_IrradianceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(9, VK_SHADER_STAGE_FRAGMENT_BIT, m_ScatteringLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(10, VK_SHADER_STAGE_FRAGMENT_BIT, m_DiffuseIrradience[i].View->GetImageView(), SamplerLinear)
			.AddImageSampler(11, VK_SHADER_STAGE_FRAGMENT_BIT, m_SpecularLUT[i].Views[0]->GetImageView(), SamplerLinear)
			.AddImageSampler(12, VK_SHADER_STAGE_FRAGMENT_BIT, m_BRDFLUT[i].View->GetImageView(), SamplerLinear)
			.AddImageSampler(13, VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeShape.View->GetImageView(), SamplerRepeat)
			.AddImageSampler(14, VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeDetail.View->GetImageView(), SamplerRepeat)
			.AddImageSampler(15, VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeWeather.View->GetImageView(), SamplerRepeat)
			.AddUniformBuffer(16, VK_SHADER_STAGE_FRAGMENT_BIT, *m_CloudLayer)
			.Allocate(m_Scope);

		m_PostProcessDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsHR[i]->GetImageView(), SamplerLinear)
			.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, m_BlurViews[2 * i + 1]->GetImageView(), SamplerLinear)
			.Allocate(m_Scope);

		m_BlurDescriptors[i] = DescriptorSetDescriptor()
			.AddImageSampler(0, VK_SHADER_STAGE_COMPUTE_BIT, m_HdrViewsHR[i]->GetImageView(), SamplerPoint)
			.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_BlurViews[2 * i]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL)
			.AddStorageImage(2, VK_SHADER_STAGE_COMPUTE_BIT, m_BlurViews[2 * i + 1]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL)
			.Allocate(m_Scope);

		m_CubemapDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, m_IrradianceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, m_ScatteringLUT.View->GetImageView(), SamplerLinear)
			.Allocate(m_Scope);

		m_ConvolutionDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, m_IrradianceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, m_ScatteringLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(4, VK_SHADER_STAGE_FRAGMENT_BIT, m_CubemapLUT[i].View->GetImageView(), SamplerPoint)
			.Allocate(m_Scope);
	}

	m_CompositionPipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("composition_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(m_CompositionDescriptors[0]->GetLayout())
		.SetRenderPass(m_Scope.GetCompositionPass(), 0)
		.Construct(m_Scope);

	m_PostProcessPipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("post_process_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(m_PostProcessDescriptors[0]->GetLayout())
		.SetRenderPass(m_Scope.GetPostProcessPass(), 0)
		.Construct(m_Scope);

	m_BlurPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_BlurDescriptors[0]->GetLayout())
		.AddPushConstant(BlurConst)
		.SetShaderName("blur_comp")
		.Construct(m_Scope);

	m_CubemapPipeline = GraphicsPipelineDescriptor()
		.AddDescriptorLayout(m_CubemapDescriptors[0]->GetLayout())
		.SetShaderStage("cubemap_vert", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("cubemap_geom", VK_SHADER_STAGE_GEOMETRY_BIT)
		.SetShaderStage("cubemap_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetRenderPass(m_Scope.GetCubemapPass(), 0)
		.Construct(m_Scope);

	m_ConvolutionPipeline = GraphicsPipelineDescriptor()
		.AddDescriptorLayout(m_ConvolutionDescriptors[0]->GetLayout())
		.SetShaderStage("cubemap_vert", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("cubemap_geom", VK_SHADER_STAGE_GEOMETRY_BIT)
		.SetShaderStage("cube_convolution_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetRenderPass(m_Scope.GetCubemapPass(), 0)
		.Construct(m_Scope);

	VkPushConstantRange pushContants{};
	pushContants.size = sizeof(float);
	pushContants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	m_SpecularIBLPipeline = GraphicsPipelineDescriptor()
		.AddDescriptorLayout(m_ConvolutionDescriptors[0]->GetLayout())
		.SetShaderStage("cubemap_vert", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("cubemap_geom", VK_SHADER_STAGE_GEOMETRY_BIT)
		.SetShaderStage("cube_convolution_spec_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetRenderPass(m_Scope.GetCubemapPass(), 0)
		.AddPushConstant(pushContants)
		.Construct(m_Scope);

	m_IntegrationPipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("brdf_integrate_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetRenderPass(m_Scope.GetSimplePass(), 0)
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

	VmaAllocationCreateInfo uboAllocCreateInfo{};
	uboAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	uboAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	m_UBOBuffers.resize(m_SwapchainImages.size());
	m_UBOSets.resize(m_SwapchainImages.size());
	for (uint32_t i = 0; i < m_UBOBuffers.size(); ++i)
	{
		m_UBOBuffers[i] = std::make_unique<Buffer>(m_Scope, uboInfo, uboAllocCreateInfo);

		m_UBOSets[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_ALL, *m_UBOBuffers[i])
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

void VulkanBase::transfer_ownership(VkQueueFlagBits queue1, VkQueueFlagBits queue2, const VulkanImage* image, const VkFence fence, const VkSemaphore sem)
{
	if (m_Scope.GetQueue(queue1).GetFamilyIndex() == m_Scope.GetQueue(queue2).GetFamilyIndex())
		return;

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = image->GetImageLayout();
	barrier.newLayout = image->GetImageLayout();
	barrier.srcQueueFamilyIndex = m_Scope.GetQueue(queue1).GetFamilyIndex();
	barrier.dstQueueFamilyIndex = m_Scope.GetQueue(queue2).GetFamilyIndex();
	barrier.image = image->GetImage();
	barrier.subresourceRange = image->GetSubResourceRange();
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	vkWaitForFences(m_Scope.GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);

	::BeginOneTimeSubmitCmd(m_OwnershipBuffers[m_SwapchainIndex + (m_SwapchainImages.size() * (queue1 - 1))]);
	::BeginOneTimeSubmitCmd(m_OwnershipBuffers[m_SwapchainIndex + (m_SwapchainImages.size() * (queue2 - 1))]);

	vkCmdPipelineBarrier(m_OwnershipBuffers[m_SwapchainIndex + (m_SwapchainImages.size() * (queue1 - 1))], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
	vkCmdPipelineBarrier(m_OwnershipBuffers[m_SwapchainIndex + (m_SwapchainImages.size() * (queue2 - 1))], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

	::EndCommandBuffer(m_OwnershipBuffers[m_SwapchainIndex + (m_SwapchainImages.size() * (queue1 - 1))]);
	::EndCommandBuffer(m_OwnershipBuffers[m_SwapchainIndex + (m_SwapchainImages.size() * (queue2 - 1))]);

	m_Scope.GetQueue(queue1).Submit(m_OwnershipBuffers[m_SwapchainIndex + (m_SwapchainImages.size() * (queue1 - 1))], sem);
	m_Scope.GetQueue(queue2).Submit2(m_OwnershipBuffers[m_SwapchainIndex + (m_SwapchainImages.size() * (queue2 - 1))], fence, sem);
}
#pragma endregion

#pragma region Objects
std::unique_ptr<VulkanTexture> VulkanBase::_loadImage(const std::string& path, VkFormat format) const
{
	std::unique_ptr<VulkanTexture> Texture = std::make_unique<VulkanTexture>();

	int w, h, c;
	unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &c, 4);
	Texture->Image = create_image(m_Scope, pixels, 1, w, h, 4, format, 0);
	Texture->View = std::make_unique<VulkanImageView>(m_Scope, *Texture->Image, Texture->Image->GetSubResourceRange());
	free(pixels);

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

	const VkCommandBuffer& cmd = m_PresentBuffers[m_SwapchainIndex];
	const VkDeviceSize offsets[] = { 0 };

	m_UBOSets[m_SwapchainIndex]->BindSet(0, cmd, *gro.pipeline);

	gro.descriptorSet->BindSet(1, cmd, *gro.pipeline);
	m_TerrainDrawSet[m_SwapchainIndex]->BindSet(2, cmd, *gro.pipeline);

	gro.pipeline->PushConstants(cmd, &constants.Offset, PBRConstants::VertexSize(), 0u, VK_SHADER_STAGE_VERTEX_BIT);
	gro.pipeline->PushConstants(cmd, &constants.Color, PBRConstants::FragmentSize(), PBRConstants::VertexSize(), VK_SHADER_STAGE_FRAGMENT_BIT);
	gro.pipeline->BindPipeline(cmd);

	vkCmdBindVertexBuffers(cmd, 0, 1, &gro.mesh->GetVertexBuffer()->GetBuffer(), offsets);
	vkCmdBindIndexBuffer(cmd, gro.mesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, gro.mesh->GetIndicesCount(), 1, 0, 0, 0);
}

void VulkanBase::_drawObject(const PBRObject& gro, const PBRConstants& constants) const
{
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0)
		return;

	assert(m_InFrame, "Call BeginFrame first!");

	const VkCommandBuffer& cmd = m_PresentBuffers[m_SwapchainIndex];
	const VkDeviceSize offsets[] = { 0 };

	m_UBOSets[m_SwapchainIndex]->BindSet(0, cmd, *gro.pipeline);
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
	const VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::BillinearRepeat);

	return DescriptorSetDescriptor()
		.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, albedo.GetImageView(), SamplerRepeat)
		.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, nh.GetImageView(), SamplerRepeat)
		.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, arm.GetImageView(), SamplerRepeat)
		.Allocate(m_Scope);
}

std::unique_ptr<GraphicsPipeline> VulkanBase::create_pbr_pipeline(const DescriptorSet& set) const
{
	auto vertAttributes = MeshVertex::getAttributeDescriptions();
	auto vertBindings = MeshVertex::getBindingDescription();

	const VkPipelineColorBlendAttachmentState attachment{
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments{
		attachment,
		attachment,
		attachment
	};

	return GraphicsPipelineDescriptor()
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.SetBlendAttachments(blendAttachments.size(), blendAttachments.data())
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
	const VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::BillinearRepeat);

	return DescriptorSetDescriptor()
		.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, albedo.GetImageView(), SamplerRepeat)
		.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, nh.GetImageView(), SamplerRepeat)
		.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, arm.GetImageView(), SamplerRepeat)
		// .AddImageSampler(4, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TerrainLUT.View->GetImageView(), SamplerRepeat)
		.Allocate(m_Scope);
}

std::unique_ptr<GraphicsPipeline> VulkanBase::create_terrain_pipeline(const DescriptorSet& set, const GR::Shapes::GeoClipmap& shape) const
{
	auto vertAttributes = TerrainVertex::getAttributeDescriptions();
	auto vertBindings = TerrainVertex::getBindingDescription();

	const VkPipelineColorBlendAttachmentState attachment{
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments{
		attachment,
		attachment,
		attachment
	};

	return GraphicsPipelineDescriptor()
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.SetBlendAttachments(blendAttachments.size(), blendAttachments.data())
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
		//.SetPolygonMode(VK_POLYGON_MODE_LINE)
		.Construct(m_Scope);
}
#pragma endregion

#pragma region Precompute
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
	imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCI.imageType = VK_IMAGE_TYPE_2D;

	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.subresourceRange = subRes;

	VmaAllocationCreateInfo imageAlloc{};
	imageAlloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkSampler ImageSampler = m_Scope.GetSampler(ESamplerType::LinearRepeat);

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
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	GenDeltaSRSMLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaSRSM_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaSRSMDSO->GetLayout())
		.Construct(m_Scope);

	m_ScatteringLUT.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	m_ScatteringLUT.View = std::make_unique<VulkanImageView>(m_Scope, *m_ScatteringLUT.Image);

	SingleScatterDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView(), ImageSampler)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSM.View->GetImageView(), ImageSampler)
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

	for (uint32_t Sample = 2; Sample < 25; Sample++)
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

	m_TransmittanceLUT.Image->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ScatteringLUT.Image->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_IrradianceLUT.Image->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	return 1;
}

VkBool32 VulkanBase::volumetric_precompute() 
{
	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	VkBufferCreateInfo cloudInfo{};
	cloudInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	cloudInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	cloudInfo.size = sizeof(CloudLayerProfile);
	cloudInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	m_CloudLayer = std::make_unique<Buffer>(m_Scope, cloudInfo, allocCreateInfo);
	 
	CloudLayerProfile defaultClouds{};
	m_CloudLayer->Update(&defaultClouds, sizeof (CloudLayerProfile));

	m_VolumeShape.Image = GRNoise::GenerateCloudShapeNoise(m_Scope, { 128u, 128u, 128u }, 4u, 8u);
	m_VolumeShape.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeShape.Image);

	m_VolumeDetail.Image = GRNoise::GenerateCloudDetailNoise(m_Scope, { 32u, 32u, 32u }, 8u, 8u);
	m_VolumeDetail.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeDetail.Image);

	m_VolumeWeather.Image = GRNoise::GeneratePerlin(m_Scope, { 256u, 256u, 1u }, 16u, 4u);
	m_VolumeWeather.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeWeather.Image);

	VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::LinearRepeat); 
	VkSampler SamplerClamp = m_Scope.GetSampler(ESamplerType::LinearClamp);

	m_Volumetrics = std::make_unique<GraphicsObject>();
	m_Volumetrics->descriptorSet = DescriptorSetDescriptor()
		.AddUniformBuffer(1, VK_SHADER_STAGE_FRAGMENT_BIT, *m_CloudLayer)
		.AddImageSampler(2,  VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeShape.View->GetImageView(), SamplerRepeat)
		.AddImageSampler(3,  VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeDetail.View->GetImageView(), SamplerRepeat)
		.AddImageSampler(4,  VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeWeather.View->GetImageView(), SamplerRepeat)
		.AddImageSampler(5,  VK_SHADER_STAGE_FRAGMENT_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(6,  VK_SHADER_STAGE_FRAGMENT_BIT, m_IrradianceLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(7,  VK_SHADER_STAGE_FRAGMENT_BIT, m_ScatteringLUT.View->GetImageView(), SamplerClamp)
		.Allocate(m_Scope);

	VkPipelineColorBlendAttachmentState blendState{};
	blendState.blendEnable = VK_FALSE;
	blendState.colorBlendOp = VK_BLEND_OP_ADD;
	blendState.alphaBlendOp = VK_BLEND_OP_ADD;
	blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

	m_Volumetrics->pipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("volumetric_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetBlendAttachments(1, &blendState)
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(m_Volumetrics->descriptorSet->GetLayout())
		.AddSpecializationConstant(0, Rg, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddSpecializationConstant(1, Rt, VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetCullMode(VK_CULL_MODE_FRONT_BIT)
		.SetRenderPass(m_Scope.GetLowResRenderPass(), 0)
		.Construct(m_Scope);

	return 1;
}

VkBool32 VulkanBase::terrain_init(const Buffer& VB, const GR::Shapes::GeoClipmap& shape)
{
	std::vector<uint32_t> queueFamilies = { m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex() };
	const uint32_t VertexCount = VB.GetDescriptor().range / sizeof(TerrainVertex);

	const uint32_t m = (glm::max(shape.m_VerPerRing, 7u) + 1) / 4;
	const uint32_t LUTExtent = static_cast<uint32_t>(2 * (m + 2) + 1);

	m_TerrainDispatches = VertexCount / 32 + static_cast<uint32_t>(VertexCount % 32 > 0);

	VkImageCreateInfo noiseInfo{};
	noiseInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	noiseInfo.arrayLayers = shape.m_Rings;
	noiseInfo.extent = { LUTExtent, LUTExtent, 1 };
	noiseInfo.format = VK_FORMAT_R32_SFLOAT;
	noiseInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	noiseInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(LUTExtent))) + 1;
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
	waterInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	waterInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(LUTExtent))) + 1;
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

	m_WaterSet.resize(m_SwapchainImages.size());
	m_WaterLUT.resize(m_SwapchainImages.size());
	m_TerrainLUT.resize(m_SwapchainImages.size());
	m_TerrainSet.resize(m_SwapchainImages.size());
	m_TerrainDrawSet.resize(m_SwapchainImages.size());

	for (uint32_t i = 0; i < m_TerrainLUT.size(); i++)
	{
		m_TerrainLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, noiseInfo, noiseAllocCreateInfo);
		m_TerrainLUT[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_TerrainLUT[i].Image);

		m_WaterLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, waterInfo, noiseAllocCreateInfo);
		m_WaterLUT[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_WaterLUT[i].Image);

		m_TerrainSet[i] = DescriptorSetDescriptor()
			.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_TerrainLUT[i].View->GetImageView())
			.AddStorageBuffer(1, VK_SHADER_STAGE_COMPUTE_BIT, VB)
			.Allocate(m_Scope);

		m_WaterSet[i] = DescriptorSetDescriptor()
			.AddImageSampler(0, VK_SHADER_STAGE_COMPUTE_BIT, m_TerrainLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::PointClamp))
			.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_WaterLUT[i].View->GetImageView())
			.Allocate(m_Scope);

		m_TerrainDrawSet[i] = DescriptorSetDescriptor()
			.AddImageSampler(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TerrainLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearClamp))
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_WaterLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearClamp))
			.Allocate(m_Scope);
	}

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
		.SetShaderName("terrain_noise_comp")
		.Construct(m_Scope);

	m_WaterCompute = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(m_WaterSet[0]->GetLayout())
		.AddSpecializationConstant(0, Rg)
		.AddSpecializationConstant(1, Rt)
		.AddSpecializationConstant(2, shape.m_Scale)
		.AddSpecializationConstant(3, shape.m_MinHeight)
		.AddSpecializationConstant(4, shape.m_MaxHeight)
		.SetShaderName("erosion_comp")
		.Construct(m_Scope);

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