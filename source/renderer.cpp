#include "pch.hpp"
#define GLFW_INCLUDE_VULKAN
#define VMA_IMPLEMENTATION
#define VK_KHR_swapchain
#include "renderer.hpp"
#include <stb/stb_image.h>

#ifdef INCLUDE_GUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"
#endif

extern std::string exec_path;

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

	res = setup_precompute() & res;
	res = prepare_scene() & res;
	
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

	defaultAlbedo.reset();
	defaultNormal.reset();
	defaultMetallic.reset();
	defaultRoughness.reset();
	defaultAO.reset();

	registry.clear<PBRObject,
		GRComponents::AlbedoMap,
		GRComponents::NormalMap,
		GRComponents::RoughnessMap,
		GRComponents::MetallicMap,
		GRComponents::AmbientMap>();

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

	registry.emplace_or_replace<GRComponents::AlbedoMap>(ent, defaultAlbedo, &gro.dirty);
	registry.emplace_or_replace<GRComponents::NormalMap>(ent, defaultNormal, &gro.dirty);
	registry.emplace_or_replace<GRComponents::RoughnessMap>(ent, defaultRoughness, &gro.dirty);
	registry.emplace_or_replace<GRComponents::MetallicMap>(ent, defaultMetallic, &gro.dirty);
	registry.emplace_or_replace<GRComponents::AmbientMap>(ent, defaultAO, &gro.dirty);

	gro.mesh = mesh_path != "" ? GRFile::ImportMesh(Scope, mesh_path.c_str()) 
							   : GRShape::Cube().Generate(Scope);

	gro.descriptorSet = create_pbr_set(Scope, *defaultAlbedo, *defaultNormal, *defaultRoughness, *defaultMetallic, *defaultAO);
	gro.pipeline = create_pbr_pipeline(Scope, *gro.descriptorSet);

	return ent;
}

entt::entity VulkanBase::AddShape(const GRShape::Shape& descriptor)
{
	entt::entity ent = registry.create();
	PBRObject& gro = registry.emplace_or_replace<PBRObject>(ent);
	registry.emplace_or_replace<GRComponents::Transform>(ent);
	registry.emplace_or_replace<GRComponents::Color>(ent);
	registry.emplace_or_replace<GRComponents::RoughnessMultiplier>(ent);

	registry.emplace_or_replace<GRComponents::AlbedoMap>(ent, defaultAlbedo, &gro.dirty);
	registry.emplace_or_replace<GRComponents::NormalMap>(ent, defaultNormal, &gro.dirty);
	registry.emplace_or_replace<GRComponents::RoughnessMap>(ent, defaultRoughness, &gro.dirty);
	registry.emplace_or_replace<GRComponents::MetallicMap>(ent, defaultMetallic, &gro.dirty);
	registry.emplace_or_replace<GRComponents::AmbientMap>(ent, defaultAO, &gro.dirty);

	gro.mesh = descriptor.Generate(Scope);
	gro.descriptorSet = create_pbr_set(Scope, *defaultAlbedo, *defaultNormal, *defaultRoughness, *defaultMetallic, *defaultAO);
	gro.pipeline = create_pbr_pipeline(Scope, *gro.descriptorSet);

	return ent;
}

void VulkanBase::RegisterEntityUpdate(entt::entity ent)
{
	VulkanImage* albedo = static_cast<VulkanImage*>(registry.get<GRComponents::AlbedoMap>(ent).Get().get());
	VulkanImage* normal = static_cast<VulkanImage*>(registry.get<GRComponents::NormalMap>(ent).Get().get());
	VulkanImage* roughness = static_cast<VulkanImage*>(registry.get<GRComponents::RoughnessMap>(ent).Get().get());
	VulkanImage* metallic = static_cast<VulkanImage*>(registry.get<GRComponents::MetallicMap>(ent).Get().get());
	VulkanImage* ambient = static_cast<VulkanImage*>(registry.get<GRComponents::AmbientMap>(ent).Get().get());

	PBRObject& gro = registry.get<PBRObject>(ent);
	gro.descriptorSet = create_pbr_set(Scope, *albedo, *normal, *roughness, *metallic, *ambient);
	gro.pipeline = create_pbr_pipeline(Scope, *gro.descriptorSet);
	gro.dirty = false;
}

TAuto<DescriptorSet> VulkanBase::create_pbr_set(const RenderScope& Scope
	, const VulkanImage& albedo
	, const VulkanImage& normal
	, const VulkanImage& roughness
	, const VulkanImage& metallic
	, const VulkanImage& ao)
{
	return DescriptorSetDescriptor()
		.AddUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *ubo)
		.AddUniformBuffer(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *view)
		.AddImageSampler(2, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *Transmittance)
		.AddImageSampler(3, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *IrradianceLUT)
		.AddImageSampler(4, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *ScatteringLUT)
		.AddImageSampler(5, VK_SHADER_STAGE_FRAGMENT_BIT, albedo)
		.AddImageSampler(6, VK_SHADER_STAGE_FRAGMENT_BIT, normal)
		.AddImageSampler(7, VK_SHADER_STAGE_FRAGMENT_BIT, roughness)
		.AddImageSampler(8, VK_SHADER_STAGE_FRAGMENT_BIT, metallic)
		.AddImageSampler(9, VK_SHADER_STAGE_FRAGMENT_BIT, ao)
		.Allocate(Scope);
}

TAuto<Pipeline> VulkanBase::create_pbr_pipeline(const RenderScope& Scope, const DescriptorSet& set)
{
	auto vertAttributes = Vertex::getAttributeDescriptions();
	auto vertBindings = Vertex::getBindingDescription();

	return GraphicsPipelineDescriptor()
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.SetVertexInputBindings(1, &vertBindings)
		.SetVertexAttributeBindings(vertAttributes.size(), vertAttributes.data())
		.SetShaderStage("default_vert", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("default_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(set.GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PBRConstants::World) })
		.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PBRConstants::World),  sizeof(PBRConstants) - sizeof(PBRConstants::World) })
		.Construct(Scope);
};

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
	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	createInfo.ppEnabledLayerNames = validationLayers.data();
	populateDebugMessengerCreateInfo(debugCreateInfo);
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = VK_NULL_HANDLE;
#endif
	return vkCreateInstance(&createInfo, VK_NULL_HANDLE, &instance) == VK_SUCCESS;
}

VkBool32 VulkanBase::create_swapchain_images()
{
	assert(Scope.GetSwapchain() != VK_NULL_HANDLE);

	uint32_t imagesCount = Scope.GetMaxFramesInFlight();
	swapchainImages.resize(imagesCount);
	depthAttachments.resize(imagesCount);
	hdrAttachments.resize(imagesCount);
	VkBool32 res = vkGetSwapchainImagesKHR(Scope.GetDevice(), Scope.GetSwapchain(), &imagesCount, swapchainImages.data()) == VK_SUCCESS;

	const TArray<uint32_t, 2> queueIndices = { Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex() };
	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	for (size_t i = 0; i < swapchainImages.size(); i++) 
	{
		VkImageView imageView;

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = swapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.format = Scope.GetColorFormat();

		VkImageCreateInfo hdrInfo{};
		hdrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		hdrInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		hdrInfo.arrayLayers = 1;
		hdrInfo.extent = { Scope.GetSwapchainExtent().width, Scope.GetSwapchainExtent().height, 1 };
		hdrInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		hdrInfo.imageType = VK_IMAGE_TYPE_2D;
		hdrInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		hdrInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		hdrInfo.mipLevels = 1;
		hdrInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		hdrInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		hdrInfo.queueFamilyIndexCount = queueIndices.size();
		hdrInfo.pQueueFamilyIndices = queueIndices.data();

		VkImageCreateInfo depthInfo{};
		depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		depthInfo.format = Scope.GetDepthFormat();
		depthInfo.arrayLayers = 1;
		depthInfo.extent = { Scope.GetSwapchainExtent().width, Scope.GetSwapchainExtent().height, 1 };
		depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		depthInfo.imageType = VK_IMAGE_TYPE_2D;
		depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		depthInfo.mipLevels = 1;
		depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		depthInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		depthInfo.queueFamilyIndexCount = queueIndices.size();
		depthInfo.pQueueFamilyIndices = queueIndices.data();

		res = (vkCreateImageView(Scope.GetDevice(), &viewInfo, VK_NULL_HANDLE, &imageView) == VK_SUCCESS) & res;
		swapchainViews.push_back(imageView);

		viewInfo.format = hdrInfo.format;
		hdrAttachments[i] = std::make_unique<VulkanImage>(Scope);
		hdrAttachments[i]->CreateImage(hdrInfo, allocCreateInfo)
			.CreateImageView(viewInfo)
			.TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.format = Scope.GetDepthFormat();
		depthAttachments[i] = std::make_unique<VulkanImage>(Scope);
		depthAttachments[i]->CreateImage(depthInfo, allocCreateInfo)
			.CreateImageView(viewInfo);
	}

	return res;
}

VkBool32 VulkanBase::create_framebuffers()
{
	assert(swapchainImages.size() > 0 && Scope.GetRenderPass() != VK_NULL_HANDLE);

	framebuffers.resize(swapchainImages.size());

	VkBool32 res = 1;
	for (size_t i = 0; i < framebuffers.size(); i++) 
	{
		res = CreateFramebuffer(Scope.GetDevice(), Scope.GetRenderPass(), Scope.GetSwapchainExtent(), { hdrAttachments[i]->GetImageView(), swapchainViews[i], depthAttachments[i]->GetImageView() }, &framebuffers[i]) & res;
	}

	return res;
}

VkBool32 VulkanBase::create_hdr_pipeline()
{
	VkBool32 res = 1;
	HDRPipelines.resize(swapchainImages.size());
	HDRDescriptors.resize(swapchainImages.size());

	for (size_t i = 0; i < HDRDescriptors.size(); i++)
	{
		HDRDescriptors[i] = DescriptorSetDescriptor()
			.AddSubpassAttachment(0, VK_SHADER_STAGE_FRAGMENT_BIT, *hdrAttachments[i])
			.Allocate(Scope);

		HDRPipelines[i] = GraphicsPipelineDescriptor()
			.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
			.SetShaderStage("hdr_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
			.AddDescriptorLayout(HDRDescriptors[i]->GetLayout())
			.SetSubpass(1)
			.Construct(Scope);
	}

	return res;
}

VkBool32 VulkanBase::prepare_scene()
{
	VkBool32 res = 1;
	auto vertAttributes = Vertex::getAttributeDescriptions();
	auto vertBindings = Vertex::getBindingDescription();

	VkBufferCreateInfo uboInfo{};
	uboInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	uboInfo.size = sizeof(UniformBuffer);
	uboInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo uboAllocCreateInfo{};
	uboAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	uboAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	ubo = std::make_unique<Buffer>(Scope, uboInfo, uboAllocCreateInfo);

	VkBufferCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	viewInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	viewInfo.size = sizeof(ViewBuffer);
	viewInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	view = std::make_unique<Buffer>(Scope, viewInfo, uboAllocCreateInfo);

	VkBufferCreateInfo cloudInfo{};
	cloudInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	cloudInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	cloudInfo.size = sizeof(CloudLayer);
	cloudInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	cloud_layer = std::make_unique<Buffer>(Scope, cloudInfo, uboAllocCreateInfo);

	CloudShape = GRNoise::GenerateCloudShapeNoise(Scope, { 128u, 128u, 128u }, 4u, 4u);
	CloudDetail = GRNoise::GenerateCloudDetailNoise(Scope, { 32u, 32u, 32u }, 8u, 3u);

	volume = std::make_unique<GraphicsObject>();
	volume->descriptorSet = DescriptorSetDescriptor()
		.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *ubo)
		.AddUniformBuffer(1, VK_SHADER_STAGE_FRAGMENT_BIT, *view)
		.AddUniformBuffer(2, VK_SHADER_STAGE_FRAGMENT_BIT, *cloud_layer)
		.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, *CloudShape)
		.AddImageSampler(4, VK_SHADER_STAGE_FRAGMENT_BIT, *CloudDetail)
		.AddImageSampler(5, VK_SHADER_STAGE_FRAGMENT_BIT, *Transmittance)
		.AddImageSampler(6, VK_SHADER_STAGE_FRAGMENT_BIT, *IrradianceLUT)
		.AddImageSampler(7, VK_SHADER_STAGE_FRAGMENT_BIT, *ScatteringLUT)
		.Allocate(Scope);

	VkPipelineColorBlendAttachmentState blendState{};
	blendState.blendEnable = true;
	blendState.colorBlendOp = VK_BLEND_OP_ADD;
	blendState.alphaBlendOp = VK_BLEND_OP_ADD;
	blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendState.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;

	volume->pipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("volumetric_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetBlendAttachments(1, &blendState)
		.AddDescriptorLayout(volume->descriptorSet->GetLayout())
		.SetCullMode(VK_CULL_MODE_FRONT_BIT)
		.Construct(Scope);
	
	skybox = std::make_unique<GraphicsObject>();
	skybox->descriptorSet = DescriptorSetDescriptor()
		.AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT, *ubo)
		.AddUniformBuffer(1, VK_SHADER_STAGE_FRAGMENT_BIT, *view)
		.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, *ScatteringLUT)
		.Allocate(Scope);

	skybox->pipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("background_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetCullMode(VK_CULL_MODE_NONE)
		.AddDescriptorLayout(skybox->descriptorSet->GetLayout())
		.Construct(Scope);

	defaultAlbedo = std::shared_ptr<VulkanImage>(GRNoise::GenerateSolidColor(Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_SRGB, std::byte(255u), std::byte(255u), std::byte(255u), std::byte(255u)));
	defaultNormal = std::shared_ptr<VulkanImage>(GRNoise::GenerateSolidColor(Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(127u), std::byte(127u), std::byte(255u), std::byte(255u)));
	defaultRoughness = std::shared_ptr<VulkanImage>(GRNoise::GenerateSolidColor(Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(255u)));
	defaultMetallic = std::shared_ptr<VulkanImage>(GRNoise::GenerateSolidColor(Scope, { 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, std::byte(0u)));
	defaultAO = defaultAlbedo;

	return res;
}

void VulkanBase::render_objects(VkCommandBuffer cmd)
{
	auto view = registry.view<PBRObject, GRComponents::Transform>();

	VkDeviceSize offsets[] = { 0 };
	for (const auto& [ent, gro, world] : view.each())
	{
		if (gro.dirty)
		{
			RegisterEntityUpdate(ent);
		}

		PBRConstants C{};
		C.World = world.matrix;
		C.Color = glm::vec4(registry.get<GRComponents::Color>(ent).RGB, 1.0);
		C.RoughnessMultiplier = registry.get<GRComponents::RoughnessMultiplier>(ent).R;

		gro.descriptorSet->BindSet(cmd, *gro.pipeline);
		gro.pipeline->PushConstants(cmd, &C.World, sizeof(PBRConstants::World), 0u, VK_SHADER_STAGE_VERTEX_BIT);
		gro.pipeline->PushConstants(cmd, &C.Color, sizeof(PBRConstants) - sizeof(PBRConstants::World), offsetof(PBRConstants, Color), VK_SHADER_STAGE_FRAGMENT_BIT);
		gro.pipeline->BindPipeline(cmd);

		vkCmdBindVertexBuffers(cmd, 0, 1, &gro.mesh->GetVertexBuffer()->GetBuffer(), offsets);
		vkCmdBindIndexBuffer(cmd, gro.mesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, gro.mesh->GetIndicesCount(), 1, 0, 0, 0);
	}
}

VkBool32 VulkanBase::setup_precompute()
{
	TAuto<VulkanImage> DeltaE;
	TAuto<VulkanImage> DeltaSR;
	TAuto<VulkanImage> DeltaSM;
	TAuto<VulkanImage> DeltaJ;

	TAuto<DescriptorSet> TrDSO;
	TAuto<DescriptorSet> DeltaEDSO;
	TAuto<DescriptorSet> DeltaSRSMDSO;
	TAuto<DescriptorSet> SingleScatterDSO;

	TAuto<DescriptorSet> DeltaJDSO;
	TAuto<DescriptorSet> DeltaEnDSO;
	TAuto<DescriptorSet> DeltaSDSO;
	TAuto<DescriptorSet> AddEDSO;
	TAuto<DescriptorSet> AddSDSO;

	TAuto<Pipeline> GenTrLUT;
	TAuto<Pipeline> GenDeltaELUT;
	TAuto<Pipeline> GenDeltaSRSMLUT;
	TAuto<Pipeline> GenSingleScatterLUT;
	TAuto<Pipeline> GenDeltaJLUT;
	TAuto<Pipeline> GenDeltaEnLUT;
	TAuto<Pipeline> GenDeltaSLUT;
	TAuto<Pipeline> AddE;
	TAuto<Pipeline> AddS;

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
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

	Transmittance = std::make_unique<VulkanImage>(Scope);
	Transmittance->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::LinearClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	TrDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *Transmittance)
		.Allocate(Scope);

	GenTrLUT = ComputePipelineDescriptor()
		.SetShaderName("transmittance_comp")
		.AddDescriptorLayout(TrDSO->GetLayout())
		.Construct(Scope);

	imageCI.extent = { 64u, 16u, 1u };
	DeltaE = std::make_unique<VulkanImage>(Scope);
	DeltaE->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::LinearClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	DeltaEDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaE)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *Transmittance)
		.Allocate(Scope);

	GenDeltaELUT = ComputePipelineDescriptor()
		.SetShaderName("deltaE_comp")
		.AddDescriptorLayout(DeltaEDSO->GetLayout())
		.Construct(Scope);

	IrradianceLUT = std::make_unique<VulkanImage>(Scope);
	IrradianceLUT->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::LinearClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	imageCI.imageType = VK_IMAGE_TYPE_3D;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_3D;
	imageCI.extent = { 256u, 128u, 32u };
	DeltaSR = std::make_unique<VulkanImage>(Scope);
	DeltaSR->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::PointClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	DeltaSM = std::make_unique<VulkanImage>(Scope);
	DeltaSM->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::PointClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	DeltaSRSMDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSM)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, *Transmittance)
		.Allocate(Scope);

	GenDeltaSRSMLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaSRSM_comp")
		.AddDescriptorLayout(DeltaSRSMDSO->GetLayout())
		.Construct(Scope);

	ScatteringLUT = std::make_unique<VulkanImage>(Scope);
	ScatteringLUT->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::LinearClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	SingleScatterDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *ScatteringLUT)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSM)
		.Allocate(Scope);

	GenSingleScatterLUT = ComputePipelineDescriptor()
		.SetShaderName("singleScattering_comp")
		.AddDescriptorLayout(SingleScatterDSO->GetLayout())
		.Construct(Scope);

	DeltaJ = std::make_unique<VulkanImage>(Scope);
	DeltaJ->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::LinearClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	DeltaJDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaJ)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *Transmittance)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaE)
		.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSM)
		.Allocate(Scope);

	GenDeltaJLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaJ_comp")
		.AddDescriptorLayout(DeltaJDSO->GetLayout())
		.Construct(Scope);

	DeltaEnDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaE)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSM)
		.Allocate(Scope);

	GenDeltaEnLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaEn_comp")
		.AddDescriptorLayout(DeltaEnDSO->GetLayout())
		.Construct(Scope);

	DeltaSDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *Transmittance)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaJ)
		.Allocate(Scope);

	GenDeltaSLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaS_comp")
		.AddDescriptorLayout(DeltaSDSO->GetLayout())
		.Construct(Scope);

	AddEDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *IrradianceLUT)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaE)
		.Allocate(Scope);

	AddE = ComputePipelineDescriptor()
		.SetShaderName("addE_comp")
		.AddDescriptorLayout(AddEDSO->GetLayout())
		.Construct(Scope);

	AddSDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *ScatteringLUT)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.Allocate(Scope);

	AddS = ComputePipelineDescriptor()
		.SetShaderName("addS_comp")
		.AddDescriptorLayout(AddSDSO->GetLayout())
		.Construct(Scope);

	VkCommandBuffer cmd;
	const Queue& Queue = Scope.GetQueue(VK_QUEUE_COMPUTE_BIT);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_COMPUTE_BIT;
	barrier.dstQueueFamilyIndex = VK_QUEUE_COMPUTE_BIT;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	Queue.AllocateCommandBuffers(1, &cmd);
	::BeginOneTimeSubmitCmd(cmd);

	GenTrLUT->BindPipeline(cmd);
	TrDSO->BindSet(cmd, *GenTrLUT);
	vkCmdDispatch(cmd, Transmittance->GetExtent().width / 8u + uint32_t(Transmittance->GetExtent().width % 8u > 0)
		, Transmittance->GetExtent().height / 8u + uint32_t(Transmittance->GetExtent().height % 8u > 0)
		, 1u);

	barrier.image = Transmittance->GetImage();
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	GenDeltaELUT->BindPipeline(cmd);
	DeltaEDSO->BindSet(cmd, *GenDeltaELUT);
	vkCmdDispatch(cmd, DeltaE->GetExtent().width / 8u + uint32_t(DeltaE->GetExtent().width % 8u > 0),
		DeltaE->GetExtent().height / 8u + uint32_t(DeltaE->GetExtent().width % 8u > 0),
		1u);

	barrier.image = DeltaE->GetImage();
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	GenDeltaSRSMLUT->BindPipeline(cmd);
	DeltaSRSMDSO->BindSet(cmd, *GenDeltaSRSMLUT);
	vkCmdDispatch(cmd, DeltaSR->GetExtent().width / 4u + uint32_t(DeltaSR->GetExtent().width % 4u > 0),
		DeltaSR->GetExtent().height / 4u + uint32_t(DeltaSR->GetExtent().height % 4u > 0),
		DeltaSR->GetExtent().depth / 4u + uint32_t(DeltaSR->GetExtent().depth % 4u > 0));

	barrier.image = DeltaSM->GetImage();
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	GenSingleScatterLUT->BindPipeline(cmd);
	SingleScatterDSO->BindSet(cmd, *GenSingleScatterLUT);
	vkCmdDispatch(cmd, ScatteringLUT->GetExtent().width / 4u + uint32_t(ScatteringLUT->GetExtent().width % 4u > 0),
		ScatteringLUT->GetExtent().height / 4u + uint32_t(ScatteringLUT->GetExtent().height % 4u > 0),
		ScatteringLUT->GetExtent().depth / 4u + uint32_t(ScatteringLUT->GetExtent().depth % 4u > 0));

	GenDeltaJLUT->BindPipeline(cmd);
	DeltaJDSO->BindSet(cmd, *GenDeltaJLUT);
	vkCmdDispatch(cmd, DeltaJ->GetExtent().width / 4u + uint32_t(DeltaJ->GetExtent().width % 4u > 0),
		DeltaJ->GetExtent().height / 4u + uint32_t(DeltaJ->GetExtent().height % 4u > 0),
		DeltaJ->GetExtent().depth / 4u + uint32_t(DeltaJ->GetExtent().depth % 4u > 0));

	GenDeltaEnLUT->BindPipeline(cmd);
	DeltaEnDSO->BindSet(cmd, *GenDeltaEnLUT);
	vkCmdDispatch(cmd, DeltaE->GetExtent().width / 8u + uint32_t(DeltaE->GetExtent().width % 8u > 0),
		DeltaE->GetExtent().height / 8u + uint32_t(DeltaE->GetExtent().width % 8u > 0),
		1u);

	GenDeltaSLUT->BindPipeline(cmd);
	DeltaSDSO->BindSet(cmd, *GenDeltaSLUT);
	vkCmdDispatch(cmd, DeltaSR->GetExtent().width / 4u + uint32_t(DeltaSR->GetExtent().width % 4u > 0),
		DeltaSR->GetExtent().height / 4u + uint32_t(DeltaSR->GetExtent().height % 4u > 0),
		DeltaSR->GetExtent().depth / 4u + uint32_t(DeltaSR->GetExtent().depth % 4u > 0));

	AddE->BindPipeline(cmd);
	AddEDSO->BindSet(cmd, *AddE);
	vkCmdDispatch(cmd, DeltaE->GetExtent().width / 8u + uint32_t(DeltaE->GetExtent().width % 8u > 0),
		DeltaE->GetExtent().height / 8u + uint32_t(DeltaE->GetExtent().width % 8u > 0),
		1u);

	AddS->BindPipeline(cmd);
	AddSDSO->BindSet(cmd, *AddS);
	vkCmdDispatch(cmd, DeltaSR->GetExtent().width / 4u + uint32_t(DeltaSR->GetExtent().width % 4u > 0),
		DeltaSR->GetExtent().height / 4u + uint32_t(DeltaSR->GetExtent().height % 4u > 0),
		DeltaSR->GetExtent().depth / 4u + uint32_t(DeltaSR->GetExtent().depth % 4u > 0));

	::EndCommandBuffer(cmd);
	Queue.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	return 1;
}

TVector<const char*> VulkanBase::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	TVector<const char*> rqextensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef VALIDATION
	rqextensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	return rqextensions;
}