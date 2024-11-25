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
	assert(pixels != nullptr && count > 0 && w > 0 && h > 0 && c > 0);

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
	VkPhysicalDeviceFeatures deviceFeatures{};
	std::vector<VkDescriptorPoolSize> poolSizes(3);
	deviceFeatures.imageCubeArray = VK_TRUE;
	deviceFeatures.fullDrawIndexUint32 = VK_TRUE;
	deviceFeatures.shaderFloat64 = VK_TRUE;
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.independentBlend = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;

	poolSizes[0].descriptorCount = 100u;
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;
	poolSizes[1].descriptorCount = 100u;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = 100u;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkBool32 res = create_instance();
	
	res = (glfwCreateWindowSurface(m_VkInstance, m_GlfwWindow, VK_NULL_HANDLE, &m_Surface) == VK_SUCCESS) & res;

	m_Scope.CreatePhysicalDevice(m_VkInstance, m_ExtensionsList)
		.CreateLogicalDevice(deviceFeatures, m_ExtensionsList, { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_COMPUTE_BIT })
		.CreateMemoryAllocator(m_VkInstance)
		.CreateSwapchain(m_Surface)
		.CreateDefaultRenderPass()
		.CreateLowResRenderPass()
		.CreateCompositionRenderPass()
		.CreatePostProcessRenderPass()
		.CreateDescriptorPool(100u, poolSizes);
	
	res = create_swapchain_images() & res;
	res = create_framebuffers() & res;

	m_Camera.Projection.SetAspect(static_cast<float>(m_Scope.GetSwapchainExtent().width) / static_cast<float>(m_Scope.GetSwapchainExtent().height))
		.SetFOV(glm::radians(45.f))
		.SetDepthRange(1e-2f, 1e4f);

	m_PresentBuffers.resize(m_SwapchainImages.size());
	m_PresentSemaphores.resize(m_SwapchainImages.size());
	m_SwapchainSemaphores.resize(m_SwapchainImages.size());
	m_PresentFences.resize(m_SwapchainImages.size());
	
	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers(m_PresentBuffers.size(), m_PresentBuffers.data());

	std::for_each(m_PresentSemaphores.begin(), m_PresentSemaphores.end(), [&, this](VkSemaphore& it) {
			res = CreateSemaphore(m_Scope.GetDevice(),  &it) & res;
	});

	std::for_each(m_SwapchainSemaphores.begin(), m_SwapchainSemaphores.end(), [&, this](VkSemaphore& it) {
		res = CreateSemaphore(m_Scope.GetDevice(), &it) & res;
	});

	std::for_each(m_PresentFences.begin(), m_PresentFences.end(), [&, this](VkFence& it) {
		res = CreateFence(m_Scope.GetDevice(), &it, VK_TRUE) & res;
	});

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
	vkWaitForFences(m_Scope.GetDevice(), m_PresentFences.size(), m_PresentFences.data(), VK_TRUE, UINT64_MAX);

#ifdef INCLUDE_GUI
	ImGui_ImplVulkan_Shutdown();
	vkDestroyDescriptorPool(m_Scope.GetDevice(), m_ImguiPool, VK_NULL_HANDLE);
#endif

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

	std::erase_if(m_PresentFences, [&, this](VkFence& it) {
		vkDestroyFence(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
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

	std::erase_if(m_SwapchainViews, [&, this](VkImageView& view) {
		vkDestroyImageView(m_Scope.GetDevice(), view, VK_NULL_HANDLE);
		return true;
	});

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.FreeCommandBuffers(m_PresentBuffers.size(), m_PresentBuffers.data());

	m_DepthAttachmentsHR.resize(0);
	m_DepthViewsHR.resize(0);

	m_NormalAttachments.resize(0);
	m_NormalViews.resize(0);

	m_DeferredAttachments.resize(0);
	m_DeferredViews.resize(0);

	m_HdrAttachmentsHR.resize(0);
	m_HdrViewsHR.resize(0);

	m_HdrAttachmentsLR.resize(0);
	m_HdrViewsLR.resize(0);

	m_DepthAttachmentsLR.resize(0);
	m_DepthViewsLR.resize(0);

	m_HDRDescriptors.resize(0);
	m_HDRPipelines.resize(0);

	m_CompositionDescriptors.resize(0);
	m_CompositionPipelines.resize(0);

	m_PostProcessDescriptors.resize(0);
	m_PostProcessPipelines.resize(0);

	m_UBOSets.resize(0);

	m_Scope.Destroy();

	vkDestroySurfaceKHR(m_VkInstance, m_Surface, VK_NULL_HANDLE);
	vkDestroyInstance(m_VkInstance, VK_NULL_HANDLE);
}

bool VulkanBase::BeginFrame()
{
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0)
		return false;

	vkWaitForFences(m_Scope.GetDevice(), 1, &m_PresentFences[m_SwapchainIndex], VK_TRUE, UINT64_MAX);
	vkResetFences(m_Scope.GetDevice(), 1, &m_PresentFences[m_SwapchainIndex]);

	assert(!m_InFrame, "Finish the frame in progress first!");

	vkAcquireNextImageKHR(m_Scope.GetDevice(), m_Scope.GetSwapchain(), UINT64_MAX, m_SwapchainSemaphores[m_SwapchainIndex], VK_NULL_HANDLE, &m_SwapchainIndex);

	const VkCommandBuffer& cmd = m_PresentBuffers[m_SwapchainIndex];

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(cmd, &beginInfo);

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
			projection_matrix,
			projection_matrix_inverse,
			CameraPositionFP64,
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

	// Prepare resources
	{
		std::array<VkImageMemoryBarrier, 4> barriers = {};
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
		barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
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
		barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
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
		barriers[2].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
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
		barriers[3].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barriers[3].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 2, &barriers[0]);
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 2, &barriers[2]);
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
		renderPassInfo.renderArea.extent = { m_Scope.GetSwapchainExtent().width / 2, m_Scope.GetSwapchainExtent().height / 2 };
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues;

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(m_Scope.GetSwapchainExtent().width / 2);
		viewport.height = static_cast<float>(m_Scope.GetSwapchainExtent().height / 2);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { m_Scope.GetSwapchainExtent().width / 2, m_Scope.GetSwapchainExtent().height / 2 };
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_UBOSets[m_SwapchainIndex]->BindSet(0, cmd, *m_Volumetrics->pipeline);
		m_Volumetrics->descriptorSet->BindSet(1, cmd, *m_Volumetrics->pipeline);
		m_Volumetrics->pipeline->BindPipeline(cmd);
		vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdEndRenderPass(cmd);
	}

	{
#ifdef INCLUDE_GUI
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
#endif

		std::array<VkClearValue, 4> clearValues;
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
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
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Scope.GetSwapchainExtent();
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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

	const VkCommandBuffer& cmd = m_PresentBuffers[m_SwapchainIndex];

	// Draw High Resolution Objects
	{
		vkCmdEndRenderPass(cmd);
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
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Scope.GetSwapchainExtent();
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_CompositionDescriptors[m_SwapchainIndex]->BindSet(0, cmd, *m_CompositionPipelines[m_SwapchainIndex]);
		m_CompositionPipelines[m_SwapchainIndex]->BindPipeline(cmd);
		vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);

		m_HDRDescriptors[m_SwapchainIndex]->BindSet(0, cmd, *m_HDRPipelines[m_SwapchainIndex]);
		m_HDRPipelines[m_SwapchainIndex]->BindPipeline(cmd);
		vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdEndRenderPass(cmd);
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
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Scope.GetSwapchainExtent();
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_PostProcessDescriptors[m_SwapchainIndex]->BindSet(0, cmd, *m_PostProcessPipelines[m_SwapchainIndex]);
		m_PostProcessPipelines[m_SwapchainIndex]->BindPipeline(cmd);
		vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);

#ifdef INCLUDE_GUI
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
#endif

		vkCmdEndRenderPass(cmd);
	}

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submitInfo{};
	std::array<VkSemaphore, 1> waitSemaphores = { m_SwapchainSemaphores[m_SwapchainIndex] };
	std::array<VkSemaphore, 1> signalSemaphores = { m_PresentSemaphores[m_SwapchainIndex] };

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = waitSemaphores.size();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_PresentBuffers[m_SwapchainIndex];
	submitInfo.signalSemaphoreCount = signalSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	VkResult res = vkQueueSubmit(m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, m_PresentFences[m_SwapchainIndex]);

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
	vkWaitForFences(m_Scope.GetDevice(), m_PresentFences.size(), m_PresentFences.data(), VK_TRUE, UINT64_MAX);

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

	std::erase_if(m_SwapchainViews, [&, this](VkImageView& view) {
		vkDestroyImageView(m_Scope.GetDevice(), view, VK_NULL_HANDLE);
		return true;
	});

	m_HdrAttachmentsHR.resize(0);
	m_HdrViewsHR.resize(0);

	m_DeferredAttachments.resize(0);
	m_DeferredViews.resize(0);

	m_NormalAttachments.resize(0);
	m_NormalViews.resize(0);

	m_DepthAttachmentsHR.resize(0);
	m_DepthViewsHR.resize(0);

	m_HdrAttachmentsLR.resize(0);
	m_HdrViewsLR.resize(0);

	m_DepthAttachmentsLR.resize(0);
	m_DepthViewsLR.resize(0);

	m_SwapchainImages.resize(0);
	m_HDRPipelines.resize(0);
	m_HDRDescriptors.resize(0);

	m_Scope.RecreateSwapchain(m_Surface);

	if (m_Scope.GetSwapchain() == VK_NULL_HANDLE)
		return;

	create_swapchain_images();
	create_framebuffers();
	create_frame_pipelines();

	m_Camera.Projection.SetAspect(static_cast<float>(m_Scope.GetSwapchainExtent().width) / static_cast<float>(m_Scope.GetSwapchainExtent().height));

	std::for_each(m_PresentSemaphores.begin(), m_PresentSemaphores.end(), [&, this](VkSemaphore& it) {
		vkDestroySemaphore(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		CreateSemaphore(m_Scope.GetDevice(), &it);
	});

	std::for_each(m_SwapchainSemaphores.begin(), m_SwapchainSemaphores.end(), [&, this](VkSemaphore& it) {
		vkDestroySemaphore(m_Scope.GetDevice(), it, VK_NULL_HANDLE);
		CreateSemaphore(m_Scope.GetDevice(), &it);
	});

	m_SwapchainIndex = 0;

	//it shouldn't change afaik, but better to keep it under control
	assert(m_SwapchainImages.size() == m_PresentBuffers.size());
}

void VulkanBase::Wait() const
{
	vkWaitForFences(m_Scope.GetDevice(), m_PresentFences.size(), m_PresentFences.data(), VK_TRUE, UINT64_MAX);
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

	m_NormalAttachments.resize(imagesCount);
	m_NormalViews.resize(imagesCount);

	m_HdrAttachmentsLR.resize(imagesCount);
	m_HdrViewsLR.resize(imagesCount);

	m_DepthAttachmentsLR.resize(imagesCount);
	m_DepthViewsLR.resize(imagesCount);

	VkBool32 res = vkGetSwapchainImagesKHR(m_Scope.GetDevice(), m_Scope.GetSwapchain(), &imagesCount, m_SwapchainImages.data()) == VK_SUCCESS;

	const std::array<uint32_t, 2> queueIndices = { m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex() };
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
		hdrInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		hdrInfo.queueFamilyIndexCount = queueIndices.size();
		hdrInfo.pQueueFamilyIndices = queueIndices.data();

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
		depthInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		depthInfo.queueFamilyIndexCount = queueIndices.size();
		depthInfo.pQueueFamilyIndices = queueIndices.data();

		res = (vkCreateImageView(m_Scope.GetDevice(), &viewInfo, VK_NULL_HANDLE, &m_SwapchainViews[i]) == VK_SUCCESS) & res;

		m_HdrAttachmentsHR[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_HdrViewsHR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_HdrAttachmentsHR[i]);

		hdrInfo.usage &= ~VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		hdrInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		m_DeferredAttachments[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_DeferredViews[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DeferredAttachments[i]);

		hdrInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		m_NormalAttachments[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_NormalViews[i] = std::make_unique<VulkanImageView>(m_Scope, *m_NormalAttachments[i]);

		m_DepthAttachmentsHR[i] = std::make_unique<VulkanImage>(m_Scope, depthInfo, allocCreateInfo);
		m_DepthViewsHR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_DepthAttachmentsHR[i]);

		hdrInfo.extent.width /= 2;
		hdrInfo.extent.height /= 2;
		hdrInfo.usage &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		hdrInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		hdrInfo.format = m_Scope.GetHDRFormat();
		m_HdrAttachmentsLR[i] = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_HdrViewsLR[i] = std::make_unique<VulkanImageView>(m_Scope, *m_HdrAttachmentsLR[i]);

		depthInfo.extent.width /= 2;
		depthInfo.extent.height /= 2;
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

	VkBool32 res = 1;
	for (size_t i = 0; i < m_SwapchainImages.size(); i++)
	{
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetRenderPass(), m_Scope.GetSwapchainExtent(), { m_HdrViewsHR[i]->GetImageView(), m_NormalViews[i]->GetImageView(),m_DeferredViews[i]->GetImageView(), m_DepthViewsHR[i]->GetImageView() }, &m_FramebuffersHR[i]) & res;
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetLowResRenderPass(), { m_Scope.GetSwapchainExtent().width / 2, m_Scope.GetSwapchainExtent().height / 2 }, { m_HdrViewsLR[i]->GetImageView(), m_DepthViewsLR[i]->GetImageView()}, &m_FramebuffersLR[i]) & res;
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetCompositionPass(), m_Scope.GetSwapchainExtent(), { m_HdrViewsHR[i]->GetImageView() }, &m_FramebuffersCP[i]);
		res &= CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetPostProcessPass(), m_Scope.GetSwapchainExtent(), { m_SwapchainViews[i] }, &m_FramebuffersPP[i]);
	}

	return res;
}

VkBool32 VulkanBase::create_frame_pipelines()
{
	VkBool32 res = 1;

	m_HDRPipelines.resize(m_SwapchainImages.size());
	m_HDRDescriptors.resize(m_SwapchainImages.size());

	m_CompositionPipelines.resize(m_SwapchainImages.size());
	m_CompositionDescriptors.resize(m_SwapchainImages.size());

	m_PostProcessPipelines.resize(m_SwapchainImages.size());
	m_PostProcessDescriptors.resize(m_SwapchainImages.size());

	VkSampler SamplerPoint = m_Scope.GetSampler(ESamplerType::PointClamp);
	VkSampler SamplerLinear = m_Scope.GetSampler(ESamplerType::LinearClamp);

	for (size_t i = 0; i < m_HDRDescriptors.size(); i++)
	{
		m_CompositionDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsHR[i]->GetImageView(), SamplerPoint, VK_IMAGE_LAYOUT_GENERAL)
			.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, m_NormalViews[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, m_DeferredViews[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(4, VK_SHADER_STAGE_FRAGMENT_BIT, m_DepthViewsHR[i]->GetImageView(), SamplerPoint)
			.AddImageSampler(5, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsLR[i]->GetImageView(), SamplerLinear)
			.AddImageSampler(6, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(7, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_IrradianceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(8, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_ScatteringLUT.View->GetImageView(), SamplerLinear)
			.Allocate(m_Scope);

		m_CompositionPipelines[i] = GraphicsPipelineDescriptor()
			.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
			.SetShaderStage("composition_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
			.AddDescriptorLayout(m_CompositionDescriptors[i]->GetLayout())
			.SetRenderPass(m_Scope.GetCompositionPass(), 0)
			.Construct(m_Scope);

		m_HDRDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *m_UBOBuffers[i])
			.AddSubpassAttachment(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsHR[i]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL)
			.Allocate(m_Scope);

		m_HDRPipelines[i] = GraphicsPipelineDescriptor()
			.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
			.SetShaderStage("hdr_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
			.AddDescriptorLayout(m_HDRDescriptors[i]->GetLayout())
			.SetRenderPass(m_Scope.GetCompositionPass(), 1)
			.Construct(m_Scope);

		m_PostProcessDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_HdrViewsHR[i]->GetImageView(), m_Scope.GetSampler(ESamplerType::PointClamp))
			.Allocate(m_Scope);

		m_PostProcessPipelines[i] = GraphicsPipelineDescriptor()
			.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
			.SetShaderStage("post_process_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
			.AddDescriptorLayout(m_PostProcessDescriptors[i]->GetLayout())
			.SetRenderPass(m_Scope.GetPostProcessPass(), 0)
			.Construct(m_Scope);
	}

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
	gro.descriptorSet = create_pbr_set(*m_DefaultWhite->View, *m_DefaultNormal->View, *m_DefaultARM->View);
	gro.pipeline = create_terrain_pipeline(*gro.descriptorSet);
	gro.mesh = shape.Generate(m_Scope);

	registry.emplace_or_replace<GR::Components::AlbedoMap>(ent, m_DefaultWhite, &gro.dirty);
	registry.emplace_or_replace<GR::Components::NormalDisplacementMap>(ent, m_DefaultNormal, &gro.dirty);
	registry.emplace_or_replace<GR::Components::AORoughnessMetallicMap>(ent, m_DefaultWhite, &gro.dirty);

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
	registry.emplace_or_replace<GR::Components::AORoughnessMetallicMap>(ent, m_DefaultWhite, &gro.dirty);

	return ent;
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
	VulkanTexture* arm = static_cast<VulkanTexture*>(registry.get<GR::Components::AORoughnessMetallicMap>(ent).Get().get());

	PBRObject& gro = registry.get<PBRObject>(ent);
	gro.descriptorSet = create_pbr_set(*albedo->View, *nh->View, *arm->View);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);
	gro.dirty = false;
}

std::unique_ptr<DescriptorSet> VulkanBase::create_pbr_set(const VulkanImageView& albedo
	, const VulkanImageView& nh
	, const VulkanImageView& arm) const
{
	VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::BillinearRepeat);

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
		.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(PBRConstants::VertexSize()),  static_cast<uint32_t>(PBRConstants::FragmentSize()) })
		.AddSpecializationConstant(0, Rg, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddSpecializationConstant(1, Rt, VK_SHADER_STAGE_FRAGMENT_BIT)
		.Construct(m_Scope);
}

std::unique_ptr<GraphicsPipeline> VulkanBase::create_terrain_pipeline(const DescriptorSet& set) const
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
		.SetShaderStage("terrain_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(set.GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<uint32_t>(PBRConstants::VertexSize()) })
		.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(PBRConstants::VertexSize()),  static_cast<uint32_t>(PBRConstants::FragmentSize()) })
		.AddSpecializationConstant(0, Rg, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddSpecializationConstant(1, Rt, VK_SHADER_STAGE_FRAGMENT_BIT)
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

	DeltaEDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, DeltaE.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	GenDeltaELUT = ComputePipelineDescriptor()
		.SetShaderName("deltaE_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaEDSO->GetLayout())
		.Construct(m_Scope);

	m_IrradianceLUT.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	m_IrradianceLUT.View = std::make_unique<VulkanImageView>(m_Scope, *m_IrradianceLUT.Image);

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
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, DeltaE.View->GetImageView(), ImageSampler)
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
	::BeginOneTimeSubmitCmd(cmd);

	GenTrLUT->BindPipeline(cmd);
	TrDSO->BindSet(0, cmd, *GenTrLUT);
	vkCmdDispatch(cmd, m_TransmittanceLUT.Image->GetExtent().width / 8u + uint32_t(m_TransmittanceLUT.Image->GetExtent().width % 8u > 0)
		, m_TransmittanceLUT.Image->GetExtent().height / 8u + uint32_t(m_TransmittanceLUT.Image->GetExtent().height % 8u > 0)
		, 1u);

	m_TransmittanceLUT.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

	GenDeltaELUT->BindPipeline(cmd);
	DeltaEDSO->BindSet(0, cmd, *GenDeltaELUT);
	vkCmdDispatch(cmd, DeltaE.Image->GetExtent().width / 8u + uint32_t(DeltaE.Image->GetExtent().width % 8u > 0),
		DeltaE.Image->GetExtent().height / 8u + uint32_t(DeltaE.Image->GetExtent().height % 8u > 0),
		1u);

	DeltaE.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

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

	for (uint32_t Sample = 2; Sample <= 10; Sample++)
	{
		DeltaJ.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		GenDeltaJLUT->PushConstants(cmd, &Sample, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		GenDeltaJLUT->BindPipeline(cmd);
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
	}

	::EndCommandBuffer(cmd);
	Queue.Submit(cmd)
		.Wait()
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

	m_VolumeShape.Image = GRNoise::GenerateCloudShapeNoise(m_Scope, { 128u, 128u, 128u }, 4u, 4u);
	m_VolumeShape.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeShape.Image);

	m_VolumeDetail.Image = GRNoise::GenerateCloudDetailNoise(m_Scope, { 32u, 32u, 32u }, 8u, 3u);
	m_VolumeDetail.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeDetail.Image);

	m_VolumeWeather.Image = GRNoise::GeneratePerlin(m_Scope, { 128u, 128u, 1u }, 32u, 1u);
	m_VolumeWeather.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeWeather.Image);

	VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::LinearRepeat);
	VkSampler SamplerClamp = m_Scope.GetSampler(ESamplerType::LinearClamp);

	m_Volumetrics = std::make_unique<GraphicsObject>();
	m_Volumetrics->descriptorSet = DescriptorSetDescriptor()
		.AddUniformBuffer(1, VK_SHADER_STAGE_FRAGMENT_BIT, *m_CloudLayer)
		.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeShape.View->GetImageView(), SamplerRepeat)
		.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeDetail.View->GetImageView(), SamplerRepeat)
		.AddImageSampler(4, VK_SHADER_STAGE_FRAGMENT_BIT, m_VolumeWeather.View->GetImageView(), SamplerRepeat)
		.AddImageSampler(5, VK_SHADER_STAGE_FRAGMENT_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(6, VK_SHADER_STAGE_FRAGMENT_BIT, m_ScatteringLUT.View->GetImageView(), SamplerClamp)
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