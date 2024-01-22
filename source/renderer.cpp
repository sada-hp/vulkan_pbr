#include "pch.hpp"
#define GLFW_INCLUDE_VULKAN
#define VMA_IMPLEMENTATION
#define VK_KHR_swapchain
#include "renderer.hpp"
#include <stb/stb_image.h>

extern std::string exec_path;

struct UniformBuffer
{
	TMat4 ViewProjection;
	TVec3 CameraPosition;
	TVec2 Aspect;
};

VulkanBase::VulkanBase(GLFWwindow* window)
	: glfwWindow(window)
{
	assert(exec_path != "");

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

	presentBuffers.resize(swapchainImages.size());
	presentSemaphores.resize(swapchainImages.size());
	presentFences.resize(swapchainImages.size());
	Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers(presentBuffers.size(), presentBuffers.data());

	std::for_each(presentSemaphores.begin(), presentSemaphores.end(), [&, this](VkSemaphore& it) {
			res = CreateSemaphore(Scope.GetDevice(),  &it) & res;
	});

	res = CreateSemaphore(Scope.GetDevice(),  &swapchainSemaphore) & res;

	std::for_each(presentFences.begin(), presentFences.end(), [&, this](VkFence& it) {
			res = CreateFence(Scope.GetDevice(), &it) & res;
	});

	res = prepare_scene() & res;

	assert(res != 0);
}

VulkanBase::~VulkanBase() noexcept
{
	vkWaitForFences(Scope.GetDevice(), presentFences.size(), presentFences.data(), VK_TRUE, UINT64_MAX);

	skybox.reset();
	volume.reset();
	ubo.reset();
	CloudShape.reset();

	std::erase_if(presentFences, [&, this](VkFence& it) {
			vkDestroyFence(Scope.GetDevice(), it, VK_NULL_HANDLE);
			return true;
		});
	vkDestroySemaphore(Scope.GetDevice(), swapchainSemaphore, VK_NULL_HANDLE);
	std::erase_if(presentSemaphores, [&, this](VkSemaphore& it) {
			vkDestroySemaphore(Scope.GetDevice(), it, VK_NULL_HANDLE);
			return true;
		});
	Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.FreeCommandBuffers(presentBuffers.size(), presentBuffers.data());
	std::erase_if(framebuffers, [&, this](VkFramebuffer& fb) {
			vkDestroyFramebuffer(Scope.GetDevice(), fb, VK_NULL_HANDLE);
			return true;
		});
	std::erase_if(swapchainViews, [&, this](VkImageView& view) {
			vkDestroyImageView(Scope.GetDevice(), view, VK_NULL_HANDLE);
			return true;
		});
	std::erase_if(depthAttachments, [&, this](std::unique_ptr<Image>& img) {
			return true;
		});

	Scope.Destroy();
	vkDestroySurfaceKHR(instance, surface, VK_NULL_HANDLE);
	vkDestroyInstance(instance, VK_NULL_HANDLE);
}

void VulkanBase::Step()
{
	if (Scope.GetSwapchainExtent().width == 0 || Scope.GetSwapchainExtent().height == 0)
		return;

	uint32_t image_index = 0;
	vkAcquireNextImageKHR(Scope.GetDevice(), Scope.GetSwapchain(), UINT64_MAX, swapchainSemaphore, VK_NULL_HANDLE, &image_index);

	vkWaitForFences(Scope.GetDevice(), 1, &presentFences[image_index], VK_TRUE, UINT64_MAX);
	vkResetFences(Scope.GetDevice(), 1, &presentFences[image_index]);

	//UBO
	{
		UniformBuffer Uniform{ camera.GetViewProjection(), camera.position, TVec2(1.0, Scope.GetSwapchainExtent().width / Scope.GetSwapchainExtent().height)};
		ubo->Update((void*)&Uniform);
	}

	//Graphics
	{

		VkResult res;
		const VkCommandBuffer& cmd = presentBuffers[image_index];
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		res = vkBeginCommandBuffer(cmd, &beginInfo);

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = framebuffers[image_index];
		renderPassInfo.renderPass = Scope.GetRenderPass();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = Scope.GetSwapchainExtent();
		renderPassInfo.clearValueCount = 2;
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

		//skybox->descriptorSet.BindSet(cmd, skybox->pipeline);
		//skybox->pipeline.BindPipeline(cmd);
		//vkCmdDraw(cmd, 36, 1, 0, 0);

		VkDeviceSize offsets[] = { 0 };
		volume->descriptorSet.BindSet(cmd, volume->pipeline);
		volume->pipeline.BindPipeline(cmd);
		vkCmdDraw(cmd, 36, 1, 0, 0);
		
		vkCmdEndRenderPass(cmd);
		vkEndCommandBuffer(cmd);
	}

	VkSubmitInfo submitInfo{};
	TArray<VkSemaphore, 1> waitSemaphores = { swapchainSemaphore };
	TArray<VkSemaphore, 1> signalSemaphores = { presentSemaphores[image_index] };

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = waitSemaphores.size();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &presentBuffers[image_index];
	submitInfo.signalSemaphoreCount = signalSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	VkResult res = vkQueueSubmit(Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, presentFences[image_index]);

	assert(res != VK_ERROR_DEVICE_LOST);

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = signalSemaphores.size();
	presentInfo.pWaitSemaphores = signalSemaphores.data();
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &Scope.GetSwapchain();
	presentInfo.pImageIndices = &image_index;
	presentInfo.pResults = VK_NULL_HANDLE;

	vkQueuePresentKHR(Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), &presentInfo);
}

void VulkanBase::HandleResize()
{
	vkWaitForFences(Scope.GetDevice(), presentFences.size(), presentFences.data(), VK_TRUE, UINT64_MAX);

	std::erase_if(framebuffers, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});
	std::erase_if(depthAttachments, [&, this](std::unique_ptr<Image>& img) {
		return true;
	});
	std::erase_if(swapchainViews, [&, this](VkImageView& view) {
		vkDestroyImageView(Scope.GetDevice(), view, VK_NULL_HANDLE);
		return true;
	});
	swapchainImages.resize(0);

	Scope.RecreateSwapchain(surface);

	if (Scope.GetSwapchain() == VK_NULL_HANDLE)
		return;

	create_swapchain_images();
	create_framebuffers();

	glm::mat4 projection = glm::perspective(30.f, static_cast<float>(Scope.GetSwapchainExtent().width) / static_cast<float>(Scope.GetSwapchainExtent().height), 0.1f, 1000.f);
	projection[1][1] *= -1;
	camera.set_projection(projection);

	//it shouldn't change afaik, but better to keep it under control
	assert(swapchainImages.size() == presentBuffers.size());
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
	appInfo.apiVersion = VK_API_VERSION_1_2;

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
	VkBool32 res = vkGetSwapchainImagesKHR(Scope.GetDevice(), Scope.GetSwapchain(), &imagesCount, swapchainImages.data()) == VK_SUCCESS;

	for (VkImage image : swapchainImages) {
		VkImageView imageView;
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.format = Scope.GetColorFormat();

		res = (vkCreateImageView(Scope.GetDevice(), &viewInfo, VK_NULL_HANDLE, &imageView) == VK_SUCCESS) & res;
		swapchainViews.push_back(imageView);
	}

	const TArray<uint32_t, 2> queueIndices = { Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex()};
	VmaAllocationCreateInfo allocCreateInfo{};
	VkImageCreateInfo depthInfo{};
	depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthInfo.format = Scope.GetDepthFormat();
	depthInfo.arrayLayers = 1;
	depthInfo.extent = { Scope.GetSwapchainExtent().width, Scope.GetSwapchainExtent().height, 1};
	depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthInfo.imageType = VK_IMAGE_TYPE_2D;
	depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthInfo.mipLevels = 1;
	depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
	depthInfo.queueFamilyIndexCount = queueIndices.size();
	depthInfo.pQueueFamilyIndices = queueIndices.data();

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.format = Scope.GetDepthFormat();

	for (auto& attachment : depthAttachments) {
		attachment = std::make_unique<Image>(Scope);
		attachment->CreateImage(depthInfo, allocCreateInfo)
				  .CreateImageView(viewInfo);
	}

	return res;
}

VkBool32 VulkanBase::create_framebuffers()
{
	assert(swapchainImages.size() > 0 && Scope.GetRenderPass() != VK_NULL_HANDLE);

	framebuffers.resize(swapchainImages.size());

	VkBool32 res = 1;
	for (size_t i = 0; i < framebuffers.size(); i++) {
		res = CreateFramebuffer(Scope.GetDevice(), Scope.GetRenderPass(), Scope.GetSwapchainExtent(), { swapchainViews[i], depthAttachments[i]->GetImageView() }, &framebuffers[i]) & res;
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

	CloudShape = GRNoise::GenerateCloudNoise(Scope, { 128u, 128u, 128u }, 4u, 4u, 4u, 4u);

	volume = std::make_unique<GraphicsObject>(Scope);
	volume->descriptorSet.AddUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *ubo)
		.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, *CloudShape)
		.Allocate();

	VkPipelineColorBlendAttachmentState blendState{};
	blendState.blendEnable = true;
	blendState.colorBlendOp = VK_BLEND_OP_ADD;
	blendState.alphaBlendOp = VK_BLEND_OP_ADD;
	blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	volume->pipeline.SetShaderStages("volumetric", (VkShaderStageFlagBits)(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT))
		.SetBlendAttachments(1, &blendState)
		.AddDescriptorLayout(volume->descriptorSet.GetLayout())
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.Construct();

	//TArray<const char*, 6> sky_collection = { ("content\\Background_East.jpg"), ("content\\Background_West.jpg"),
	//	("content\\Background_Top.jpg"), ("content\\Background_Bottom.jpg"),
	//	("content\\Background_North.jpg"), ("content\\Background_South.jpg") };

	//skybox = std::make_unique<GraphicsObject>(Scope);
	//skybox->textures.push_back(GRFile::ImportImages(Scope, sky_collection.data(), sky_collection.size(), VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT));
	//skybox->descriptorSet.AddUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT, *ubo)
	//	.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, *skybox->textures[0])
	//	.Allocate();

	//skybox->pipeline.SetShaderStages("background", (VkShaderStageFlagBits)(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT))
	//	.SetCullMode(VK_CULL_MODE_FRONT_BIT)
	//	.AddDescriptorLayout(skybox->descriptorSet.GetLayout())
	//	.Construct();

	glm::mat4 projection = glm::perspective(30.f, static_cast<float>(Scope.GetSwapchainExtent().width) / static_cast<float>(Scope.GetSwapchainExtent().height), 0.1f, 1000.f);
	projection[1][1] *= -1;
	camera.set_projection(projection);

	return res;
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