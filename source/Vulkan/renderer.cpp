#define GLFW_INCLUDE_VULKAN
#define VMA_IMPLEMENTATION
#define VK_KHR_swapchain

#include "pch.hpp"
#include "renderer.hpp"

#ifdef INCLUDE_GUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"
#endif

VulkanBase::VulkanBase(GLFWwindow* window, entt::registry& in_registry)
	: m_GlfwWindow(window), m_Registry(in_registry)
{
	VkPhysicalDeviceFeatures deviceFeatures{};
	TVector<VkDescriptorPoolSize> poolSizes(3);
	deviceFeatures.imageCubeArray = VK_TRUE;
	deviceFeatures.fullDrawIndexUint32 = VK_TRUE;
	deviceFeatures.samplerAnisotropy = VK_TRUE;
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
		.CreateDescriptorPool(100u, poolSizes);
	
	res = create_swapchain_images() & res;
	res = create_framebuffers() & res;
	res = create_hdr_pipeline() & res;

	m_Camera.Projection.SetFOV(glm::radians(45.f), static_cast<float>(m_Scope.GetSwapchainExtent().width) / static_cast<float>(m_Scope.GetSwapchainExtent().height))
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
	
#ifdef INCLUDE_GUI
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
	init_info.RenderPass = m_Scope.GetRenderPass();
	init_info.Subpass = 1;

	ImGui_ImplVulkan_Init(&init_info);
#endif

	assert(res != 0);
}

VulkanBase::~VulkanBase() noexcept
{
	vkWaitForFences(m_Scope.GetDevice(), m_PresentFences.size(), m_PresentFences.data(), VK_TRUE, UINT64_MAX);

#ifdef INCLUDE_GUI
	ImGui_ImplVulkan_Shutdown();
	vkDestroyDescriptorPool(m_Scope.GetDevice(), m_ImguiPool, VK_NULL_HANDLE);
#endif

	m_Atmospherics.reset();
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

	m_Registry.clear<PBRObject,
		GRComponents::AlbedoMap,
		GRComponents::NormalDisplacementMap,
		GRComponents::AORoughnessMetallicMap>();

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
	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT) 
		.FreeCommandBuffers(m_PresentBuffers.size(), m_PresentBuffers.data());
	std::erase_if(m_FramebuffersHR, [&, this](VkFramebuffer& fb) {
			vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
			return true;
	});
	std::erase_if(m_FramebuffersLR, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(m_Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});
	std::erase_if(m_SwapchainViews, [&, this](VkImageView& view) {
			vkDestroyImageView(m_Scope.GetDevice(), view, VK_NULL_HANDLE);
			return true;
	});

	m_DepthAttachmentsHR.resize(0);
	m_DepthViewsHR.resize(0);

	m_HdrAttachmentsHR.resize(0);
	m_HdrViewsHR.resize(0);

	m_DepthAttachmentsLR.resize(0);
	m_DepthViewsLR.resize(0);

	m_HdrAttachmentsLR.resize(0);
	m_HdrViewsLR.resize(0);

	m_HDRPipelines.resize(0);
	m_HDRDescriptors.resize(0);
	m_UBOSets.resize(0);

	m_Scope.Destroy();

	vkDestroySurfaceKHR(m_VkInstance, m_Surface, VK_NULL_HANDLE);
	vkDestroyInstance(m_VkInstance, VK_NULL_HANDLE);
}

void VulkanBase::_step(float DeltaTime)
{
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0)
		return;

	vkWaitForFences(m_Scope.GetDevice(), 1, &m_PresentFences[m_SwapchainIndex], VK_TRUE, UINT64_MAX);
	vkResetFences(m_Scope.GetDevice(), 1, &m_PresentFences[m_SwapchainIndex]);

	vkAcquireNextImageKHR(m_Scope.GetDevice(), m_Scope.GetSwapchain(), UINT64_MAX, m_SwapchainSemaphores[m_SwapchainIndex], VK_NULL_HANDLE, &m_SwapchainIndex);

	const VkCommandBuffer& cmd = m_PresentBuffers[m_SwapchainIndex];

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// Udpate UBO
	{
		TMat4 view_matrix = m_Camera.get_view_matrix();
		TMat4 projection_matrix = m_Camera.get_projection_matrix();
		TMat4 view_proj_matrix = projection_matrix * view_matrix;
		TVec4 CameraPosition = TVec4(m_Camera.View.GetOffset(), 1.0);
		TVec3 Sun = glm::normalize(m_SunDirection);
		TVec2 ScreenSize = TVec2(static_cast<float>(m_Scope.GetSwapchainExtent().width), static_cast<float>(m_Scope.GetSwapchainExtent().height));
		float Time = glfwGetTime();

		UniformBuffer Uniform
		{ 
			view_proj_matrix,
			projection_matrix,
			view_matrix,
			CameraPosition,
			Sun,
			Time,
			ScreenSize
		};

		m_UBOBuffers[m_SwapchainIndex]->Update(static_cast<void*>(&Uniform), sizeof(Uniform));
	}

	// Draw Low Resolution Background
	{
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

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
		viewport.width = (float)(m_Scope.GetSwapchainExtent().width / 2);
		viewport.height = (float)(m_Scope.GetSwapchainExtent().height / 2);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { m_Scope.GetSwapchainExtent().width / 2, m_Scope.GetSwapchainExtent().height / 2 };
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_UBOSets[m_SwapchainIndex]->BindSet(0, cmd, *m_Atmospherics->pipeline);
		m_Atmospherics->descriptorSet->BindSet(1, cmd, *m_Atmospherics->pipeline);
		m_Atmospherics->pipeline->BindPipeline(cmd);
		vkCmdDraw(cmd, 36, 1, 0, 0);

		m_UBOSets[m_SwapchainIndex]->BindSet(0, cmd, *m_Volumetrics->pipeline);
		m_Volumetrics->descriptorSet->BindSet(1, cmd, *m_Volumetrics->pipeline);
		m_Volumetrics->pipeline->BindPipeline(cmd);
		vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdEndRenderPass(cmd);
	}

	// Blit Low Resolution Image to High Resolution Attachment
	{
		TArray<VkImageMemoryBarrier, 2> barriers = {};
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
		barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

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
		barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, barriers.size(), barriers.data());

		VkImageBlit Region{};
		Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.dstSubresource.baseArrayLayer = 0;
		Region.dstSubresource.mipLevel = 0;
		Region.dstSubresource.layerCount = 1;
		Region.dstOffsets[0] = { 0, 0, 0 };
		Region.dstOffsets[1] = { int(m_Scope.GetSwapchainExtent().width), int(m_Scope.GetSwapchainExtent().height), 1 };

		Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.srcSubresource.baseArrayLayer = 0;
		Region.srcSubresource.mipLevel = 0;
		Region.srcSubresource.layerCount = 1;
		Region.srcOffsets[0] = { 0, 0, 0 };
		Region.srcOffsets[1] = { int(m_Scope.GetSwapchainExtent().width / 2), int(m_Scope.GetSwapchainExtent().height / 2), 1 };
		
		vkCmdBlitImage(cmd, m_HdrAttachmentsLR[m_SwapchainIndex]->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			m_HdrAttachmentsHR[m_SwapchainIndex]->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &Region, VK_FILTER_LINEAR);

		barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[0].image = m_HdrAttachmentsHR[m_SwapchainIndex]->GetImage();
		barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[0].subresourceRange.baseArrayLayer = 0;
		barriers[0].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barriers[0].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barriers[0].subresourceRange.baseMipLevel = 0;
		barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barriers[0]);
	}

	// Draw High Resolution Objects
	{
		VkClearValue clearValues[3];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } }; // HDR Attachment loads and doesn't use clear
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[2].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = m_FramebuffersHR[m_SwapchainIndex];
		renderPassInfo.renderPass = m_Scope.GetRenderPass();
		renderPassInfo.renderArea.offset = { 0, 0 }; 
		renderPassInfo.renderArea.extent = m_Scope.GetSwapchainExtent();
		renderPassInfo.clearValueCount = 3;
		renderPassInfo.pClearValues = clearValues;

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)m_Scope.GetSwapchainExtent().width;
		viewport.height = (float)m_Scope.GetSwapchainExtent().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Scope.GetSwapchainExtent();
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		render_objects(cmd);

		vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);

		m_HDRDescriptors[m_SwapchainIndex]->BindSet(0, cmd, *m_HDRPipelines[m_SwapchainIndex]);
		m_HDRPipelines[m_SwapchainIndex]->BindPipeline(cmd);
		vkCmdDraw(cmd, 3, 1, 0, 0);

#ifdef INCLUDE_GUI
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
#endif

		vkCmdEndRenderPass(cmd);
	}

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submitInfo{};
	TArray<VkSemaphore, 1> waitSemaphores   = { m_SwapchainSemaphores[m_SwapchainIndex] };
	TArray<VkSemaphore, 1> signalSemaphores = { m_PresentSemaphores[m_SwapchainIndex] };

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
	std::erase_if(m_SwapchainViews, [&, this](VkImageView& view) {
		vkDestroyImageView(m_Scope.GetDevice(), view, VK_NULL_HANDLE);
		return true;
	});

	m_HdrAttachmentsHR.resize(0);
	m_HdrViewsHR.resize(0);

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
	create_hdr_pipeline();

	m_Camera.Projection.SetFOV(glm::radians(45.f), static_cast<float>(m_Scope.GetSwapchainExtent().width) / static_cast<float>(m_Scope.GetSwapchainExtent().height))
		.SetDepthRange(1e-2f, 1e4f);

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

TAuto<VulkanTexture> VulkanBase::_loadImage(const std::string& path, VkFormat format)
{
	TAuto< VulkanTexture> Texture = std::make_unique<VulkanTexture>();
	Texture->Image = GRVkFile::_importImage(m_Scope, path.c_str(), format, 0);
	Texture->View = std::make_unique<VulkanImageView>(m_Scope, *Texture->Image, Texture->Image->GetSubResourceRange());

	return Texture;
}

void VulkanBase::Wait() const
{
	vkWaitForFences(m_Scope.GetDevice(), m_PresentFences.size(), m_PresentFences.data(), VK_TRUE, UINT64_MAX);
}

void VulkanBase::SetCloudLayerSettings(CloudLayerProfile settings)
{
	m_CloudLayer->Update(&settings, sizeof(CloudLayerProfile));
}

void VulkanBase::render_objects(VkCommandBuffer cmd)
{
	auto view = m_Registry.view<PBRObject, GRComponents::Transform>();

	VkDeviceSize offsets[] = { 0 };
	for (const auto& [ent, gro, world] : view.each())
	{
		if (gro.dirty)
		{
			update_pipeline(ent);
		}

		PBRConstants C{};
		C.World = world.matrix;
		C.Color = glm::vec4(m_Registry.get<GRComponents::RGBColor>(ent).Value, 1.0);
		C.RoughnessMultiplier = m_Registry.get<GRComponents::RoughnessMultiplier>(ent).Value;
		C.Metallic = m_Registry.get<GRComponents::MetallicOverride>(ent).Value;
		C.HeightScale = m_Registry.get<GRComponents::DisplacementScale>(ent).Value;

		m_UBOSets[m_SwapchainIndex]->BindSet(0, cmd, *gro.pipeline);
		gro.descriptorSet->BindSet(1, cmd, *gro.pipeline);
		gro.pipeline->PushConstants(cmd, &C.World, sizeof(PBRConstants::World), 0u, VK_SHADER_STAGE_VERTEX_BIT);
		gro.pipeline->PushConstants(cmd, &C.Color, sizeof(PBRConstants) - sizeof(PBRConstants::World), offsetof(PBRConstants, Color), VK_SHADER_STAGE_FRAGMENT_BIT);
		gro.pipeline->BindPipeline(cmd);

		vkCmdBindVertexBuffers(cmd, 0, 1, &gro.mesh->GetVertexBuffer()->GetBuffer(), offsets);
		vkCmdBindIndexBuffer(cmd, gro.mesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, gro.mesh->GetIndicesCount(), 1, 0, 0, 0);
	}
}