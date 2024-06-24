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
	: glfwWindow(window), registry(in_registry)
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
	
	res = (glfwCreateWindowSurface(instance, glfwWindow, VK_NULL_HANDLE, &surface) == VK_SUCCESS) & res;

	Scope.CreatePhysicalDevice(instance, extensions)
		.CreateLogicalDevice(deviceFeatures, extensions, { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_COMPUTE_BIT })
		.CreateMemoryAllocator(instance)
		.CreateSwapchain(surface)
		.CreateDefaultRenderPass()
		.CreateDescriptorPool(100u, poolSizes);
	
	res = create_swapchain_images() & res;
	res = create_framebuffers() & res;
	res = create_hdr_pipeline() & res;

	camera.Projection.SetFOV(glm::radians(45.f), static_cast<float>(Scope.GetSwapchainExtent().width) / static_cast<float>(Scope.GetSwapchainExtent().height))
		.SetDepthRange(1e-2f, 1e4f);

	presentBuffers.resize(swapchainImages.size());
	presentSemaphores.resize(swapchainImages.size());
	swapchainSemaphores.resize(swapchainImages.size());
	presentFences.resize(swapchainImages.size());
	Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers(presentBuffers.size(), presentBuffers.data());

	std::for_each(presentSemaphores.begin(), presentSemaphores.end(), [&, this](VkSemaphore& it) {
			res = CreateSemaphore(Scope.GetDevice(),  &it) & res;
	});

	std::for_each(swapchainSemaphores.begin(), swapchainSemaphores.end(), [&, this](VkSemaphore& it) {
		res = CreateSemaphore(Scope.GetDevice(), &it) & res;
	});

	std::for_each(presentFences.begin(), presentFences.end(), [&, this](VkFence& it) {
		res = CreateFence(Scope.GetDevice(), &it, VK_TRUE) & res;
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

	::CreateDescriptorPool(Scope.GetDevice(), pool_sizes.data(), pool_sizes.size(), 100, &imguiPool);

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = instance;
	init_info.PhysicalDevice = Scope.GetPhysicalDevice();
	init_info.Device = Scope.GetDevice();
	init_info.Queue = Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue();
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.RenderPass = Scope.GetRenderPass();
	init_info.Subpass = 1;

	ImGui_ImplVulkan_Init(&init_info);
#endif

	assert(res != 0);
}

VulkanBase::~VulkanBase() noexcept
{
	vkWaitForFences(Scope.GetDevice(), presentFences.size(), presentFences.data(), VK_TRUE, UINT64_MAX);

#ifdef INCLUDE_GUI
	ImGui_ImplVulkan_Shutdown();
	::vkDestroyDescriptorPool(Scope.GetDevice(), imguiPool, VK_NULL_HANDLE);
#endif

	skybox.reset();
	volume.reset();
	ubo.reset();
	view.reset();
	cloud_layer.reset();
	CloudShape.reset();
	CloudDetail.reset();

	Transmittance.reset();
	ScatteringLUT.reset();
	IrradianceLUT.reset();

	defaultWhite.reset();
	defaultBlack.reset();
	defaultNormal.reset();

	registry.clear<PBRObject,
		GRComponents::AlbedoMap,
		GRComponents::NormalMap,
		GRComponents::RoughnessMap,
		GRComponents::MetallicMap,
		GRComponents::AmbientMap,
		GRComponents::DisplacementMap>();

	std::erase_if(presentFences, [&, this](VkFence& it) {
			vkDestroyFence(Scope.GetDevice(), it, VK_NULL_HANDLE);
			return true;
	});
	std::erase_if(swapchainSemaphores, [&, this](VkSemaphore& it) {
		vkDestroySemaphore(Scope.GetDevice(), it, VK_NULL_HANDLE);
		return true;
	});
	std::erase_if(presentSemaphores, [&, this](VkSemaphore& it) {
			vkDestroySemaphore(Scope.GetDevice(), it, VK_NULL_HANDLE);
			return true;
	});
	Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT) .FreeCommandBuffers(presentBuffers.size(), presentBuffers.data());
	std::erase_if(framebuffers, [&, this](VkFramebuffer& fb) {
			vkDestroyFramebuffer(Scope.GetDevice(), fb, VK_NULL_HANDLE);
			return true;
	});
	std::erase_if(swapchainViews, [&, this](VkImageView& view) {
			vkDestroyImageView(Scope.GetDevice(), view, VK_NULL_HANDLE);
			return true;
	});
	depthAttachments.clear();
	hdrAttachments.clear();
	HDRPipelines.resize(0);
	HDRDescriptors.resize(0);

	Scope.Destroy();

	vkDestroySurfaceKHR(instance, surface, VK_NULL_HANDLE);
	vkDestroyInstance(instance, VK_NULL_HANDLE);
}

void VulkanBase::Step(float DeltaTime)
{
	if (Scope.GetSwapchainExtent().width == 0 || Scope.GetSwapchainExtent().height == 0)
		return;

	vkWaitForFences(Scope.GetDevice(), 1, &presentFences[swapchain_index], VK_TRUE, UINT64_MAX);
	vkResetFences(Scope.GetDevice(), 1, &presentFences[swapchain_index]);

	vkAcquireNextImageKHR(Scope.GetDevice(), Scope.GetSwapchain(), UINT64_MAX, swapchainSemaphores[swapchain_index], VK_NULL_HANDLE, &swapchain_index);

	//UBO
	{
		TMat4 view_matrix = camera.get_view_matrix();
		TMat4 projection_matrix = camera.get_projection_matrix();
		TMat4 view_proj_matrix = projection_matrix * view_matrix;

		float Time = glfwGetTime();
		UniformBuffer Uniform
		{ 
			view_proj_matrix,
			projection_matrix,
			view_matrix,
			glm::normalize(SunDirection),
			Time
		};

		ubo->Update(static_cast<void*>(&Uniform), sizeof(Uniform));
	}

	//View
	{
		TVec2 ScreenSize = TVec2(static_cast<float>(Scope.GetSwapchainExtent().width), static_cast<float>(Scope.GetSwapchainExtent().height));
		TVec4 CameraPosition = TVec4(camera.View.GetOffset(), 1.0);
		ViewBuffer Uniform
		{
			CameraPosition,
			ScreenSize
		};

		view->Update(static_cast<void*>(&Uniform));
	}

	//Cloud layer
	{
		cloud_layer->Update(&CloudLayer, sizeof(CloudProfileLayer));
	}

	//Draw
	{
		VkResult res;
		const VkCommandBuffer& cmd = presentBuffers[swapchain_index];
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		res = vkBeginCommandBuffer(cmd, &beginInfo);

		VkClearValue clearValues[3];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[2].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = framebuffers[swapchain_index];
		renderPassInfo.renderPass = Scope.GetRenderPass();
		renderPassInfo.renderArea.offset = { 0, 0 }; 
		renderPassInfo.renderArea.extent = Scope.GetSwapchainExtent();
		renderPassInfo.clearValueCount = 3;
		renderPassInfo.pClearValues = clearValues;

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)Scope.GetSwapchainExtent().width;
		viewport.height = (float)Scope.GetSwapchainExtent().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = Scope.GetSwapchainExtent();
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		render_objects(cmd);

		skybox->descriptorSet->BindSet(cmd, *skybox->pipeline);
		skybox->pipeline->BindPipeline(cmd);
		vkCmdDraw(cmd, 36, 1, 0, 0);

		//volume->descriptorSet->BindSet(cmd, *volume->pipeline);
		//volume->pipeline->BindPipeline(cmd);
		//vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);

		HDRDescriptors[swapchain_index]->BindSet(cmd, *HDRPipelines[swapchain_index]);
		HDRPipelines[swapchain_index]->BindPipeline(cmd);
		vkCmdDraw(cmd, 3, 1, 0, 0);

#ifdef INCLUDE_GUI
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
#endif

		vkCmdEndRenderPass(cmd);
		vkEndCommandBuffer(cmd);
	}

	VkSubmitInfo submitInfo{};
	TArray<VkSemaphore, 1> waitSemaphores   = { swapchainSemaphores[swapchain_index] };
	TArray<VkSemaphore, 1> signalSemaphores = { presentSemaphores[swapchain_index] };

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = waitSemaphores.size();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &presentBuffers[swapchain_index];
	submitInfo.signalSemaphoreCount = signalSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	VkResult res = vkQueueSubmit(Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, presentFences[swapchain_index]);

	assert(res != VK_ERROR_DEVICE_LOST);

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = signalSemaphores.size();
	presentInfo.pWaitSemaphores = signalSemaphores.data();
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &Scope.GetSwapchain();
	presentInfo.pImageIndices = &swapchain_index;
	presentInfo.pResults = VK_NULL_HANDLE;

	vkQueuePresentKHR(Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), &presentInfo);
	swapchain_index = (swapchain_index + 1) % swapchainImages.size();
}

void VulkanBase::HandleResize()
{
	vkWaitForFences(Scope.GetDevice(), presentFences.size(), presentFences.data(), VK_TRUE, UINT64_MAX);

	std::erase_if(framebuffers, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});
	std::erase_if(swapchainViews, [&, this](VkImageView& view) {
		vkDestroyImageView(Scope.GetDevice(), view, VK_NULL_HANDLE);
		return true;
	});
	depthAttachments.resize(0);
	swapchainImages.resize(0);
	hdrAttachments.resize(0);
	HDRPipelines.resize(0);
	HDRDescriptors.resize(0);

	Scope.RecreateSwapchain(surface);

	if (Scope.GetSwapchain() == VK_NULL_HANDLE)
		return;

	create_swapchain_images();
	create_framebuffers();
	create_hdr_pipeline();

	camera.Projection.SetFOV(glm::radians(45.f), static_cast<float>(Scope.GetSwapchainExtent().width) / static_cast<float>(Scope.GetSwapchainExtent().height))
		.SetDepthRange(1e-2f, 1e4f);

	std::for_each(presentSemaphores.begin(), presentSemaphores.end(), [&, this](VkSemaphore& it) {
		vkDestroySemaphore(Scope.GetDevice(), it, VK_NULL_HANDLE);
		CreateSemaphore(Scope.GetDevice(), &it);
	});

	std::for_each(swapchainSemaphores.begin(), swapchainSemaphores.end(), [&, this](VkSemaphore& it) {
		vkDestroySemaphore(Scope.GetDevice(), it, VK_NULL_HANDLE);
		CreateSemaphore(Scope.GetDevice(), &it);
	});

	swapchain_index = 0;

	//it shouldn't change afaik, but better to keep it under control
	assert(swapchainImages.size() == presentBuffers.size());
}

TAuto<VulkanImage> VulkanBase::LoadImage(const std::string& path, VkFormat format)
{
	return GRFile::ImportImage(Scope, path.c_str(), format, 0);
}

entt::entity VulkanBase::AddMesh(const std::string& mesh_path)
{
	entt::entity ent = registry.create();

	PBRObject& gro = registry.emplace_or_replace<PBRObject>(ent);
	registry.emplace_or_replace<GRComponents::Transform>(ent);
	registry.emplace_or_replace<GRComponents::Color>(ent);
	registry.emplace_or_replace<GRComponents::RoughnessMultiplier>(ent);
	registry.emplace_or_replace<GRComponents::MetallicOverride>(ent);

	registry.emplace_or_replace<GRComponents::AlbedoMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::NormalMap>(ent, defaultNormal, &gro.dirty);
	registry.emplace_or_replace<GRComponents::RoughnessMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::MetallicMap>(ent, defaultBlack, &gro.dirty);
	registry.emplace_or_replace<GRComponents::AmbientMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::DisplacementMap>(ent, defaultWhite, &gro.dirty);

	gro.mesh = mesh_path != "" ? GRFile::ImportMesh(Scope, mesh_path.c_str()) 
							   : GRShape::Cube().Generate(Scope);

	gro.descriptorSet = create_pbr_set(*defaultWhite, *defaultNormal, *defaultWhite, *defaultBlack, *defaultWhite, *defaultWhite);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);

	return ent;
}

entt::entity VulkanBase::AddShape(const GRShape::Shape& descriptor)
{
	entt::entity ent = registry.create();
	PBRObject& gro = registry.emplace_or_replace<PBRObject>(ent);
	registry.emplace_or_replace<GRComponents::Transform>(ent);
	registry.emplace_or_replace<GRComponents::Color>(ent);
	registry.emplace_or_replace<GRComponents::RoughnessMultiplier>(ent);
	registry.emplace_or_replace<GRComponents::MetallicOverride>(ent);

	registry.emplace_or_replace<GRComponents::AlbedoMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::NormalMap>(ent, defaultNormal, &gro.dirty);
	registry.emplace_or_replace<GRComponents::RoughnessMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::MetallicMap>(ent, defaultBlack, &gro.dirty);
	registry.emplace_or_replace<GRComponents::AmbientMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::DisplacementMap>(ent, defaultWhite, &gro.dirty);

	gro.mesh = descriptor.Generate(Scope);
	gro.descriptorSet = create_pbr_set(*defaultWhite, *defaultNormal, *defaultWhite, *defaultBlack, *defaultWhite, *defaultWhite);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);

	return ent;
}

void VulkanBase::Wait()
{
	vkWaitForFences(Scope.GetDevice(), presentFences.size(), presentFences.data(), VK_TRUE, UINT64_MAX);
}

void VulkanBase::render_objects(VkCommandBuffer cmd)
{
	auto view = registry.view<PBRObject, GRComponents::Transform>();

	VkDeviceSize offsets[] = { 0 };
	for (const auto& [ent, gro, world] : view.each())
	{
		if (gro.dirty)
		{
			update_pipeline(ent);
		}

		PBRConstants C{};
		C.World = world.matrix;
		C.Color = glm::vec4(registry.get<GRComponents::Color>(ent).RGB, 1.0);
		C.RoughnessMultiplier = registry.get<GRComponents::RoughnessMultiplier>(ent).R;
		C.Metallic = registry.get<GRComponents::MetallicOverride>(ent).M;

		gro.descriptorSet->BindSet(cmd, *gro.pipeline);
		gro.pipeline->PushConstants(cmd, &C.World, sizeof(PBRConstants::World), 0u, VK_SHADER_STAGE_VERTEX_BIT);
		gro.pipeline->PushConstants(cmd, &C.Color, sizeof(PBRConstants) - sizeof(PBRConstants::World), offsetof(PBRConstants, Color), VK_SHADER_STAGE_FRAGMENT_BIT);
		gro.pipeline->BindPipeline(cmd);

		vkCmdBindVertexBuffers(cmd, 0, 1, &gro.mesh->GetVertexBuffer()->GetBuffer(), offsets);
		vkCmdBindIndexBuffer(cmd, gro.mesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, gro.mesh->GetIndicesCount(), 1, 0, 0, 0);
	}
}