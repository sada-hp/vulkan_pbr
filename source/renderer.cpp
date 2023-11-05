#include "pch.hpp"
#define GLFW_INCLUDE_VULKAN
#define VMA_IMPLEMENTATION
#define VK_KHR_swapchain
#include "renderer.hpp"
#include <stb/stb_image.h>

extern std::string exec_path;

VulkanBase::VulkanBase(GLFWwindow* window)
	: _glfwWindow(window)
{
	assert(exec_path != "");

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.imageCubeArray = VK_TRUE;
	deviceFeatures.fullDrawIndexUint32 = VK_TRUE;
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkBool32 res = create_instance();
	
	res = (glfwCreateWindowSurface(_instance, _glfwWindow, VK_NULL_HANDLE, &_surface) == VK_SUCCESS) & res;
	
	Scope.CreatePhysicalDevice(_instance, _extensions)
		.CreateLogicalDevice(deviceFeatures, _extensions, { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_COMPUTE_BIT })
		.CreateMemoryAllocator(_instance)
		.CreateSwapchain(_surface)
		.CreateDefaultRenderPass();
	
	res = create_swapchain_images() & res;
	res = create_framebuffers() & res;

	fileManager = std::make_unique<FileManager>(Scope);

	_presentBuffers.resize(_swapchainImages.size());
	_presentSemaphores.resize(_swapchainImages.size());
	_presentFences.resize(_swapchainImages.size());
	Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).AllocateCommandBuffers(_presentBuffers.size(), _presentBuffers.data());

	std::for_each(_presentSemaphores.begin(), _presentSemaphores.end(), [&, this](VkSemaphore& it) {
			res = CreateSemaphore(Scope.GetDevice(),  &it) & res;
		});

	res = CreateSemaphore(Scope.GetDevice(),  & _swapchainSemaphore) & res;

	std::for_each(_presentFences.begin(), _presentFences.end(), [&, this](VkFence& it) {
			res = CreateFence(Scope.GetDevice(), &it) & res;
		});

	VkBufferCreateInfo uboInfo{};
	uboInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	uboInfo.size = sizeof(TransformMatrix);
	uboInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo uboAllocCreateInfo{};
	uboAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	uboAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	_ubo = std::make_unique<Buffer>(Scope, uboInfo, uboAllocCreateInfo);

	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 2;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 1;
	res = CreateDescriptorPool(Scope.GetDevice(), poolSizes.data(), poolSizes.size(), 2, &_descriptorPool) & res;

	res = prepare_scene() & res;

	glm::mat4 projection = glm::perspective(30.f, static_cast<float>(Scope.GetSwapchainExtent().width) / static_cast<float>(Scope.GetSwapchainExtent().height), 0.1f, 1000.f);
	projection[1][1] *= -1;
	camera.set_projection(projection);

	assert(res != 0);
}

VulkanBase::~VulkanBase() noexcept
{
	vkWaitForFences(Scope.GetDevice(), _presentFences.size(), _presentFences.data(), VK_TRUE, UINT64_MAX);

	swordMesh.reset();
	_skyboxImg.reset();
	_ubo.reset();
	sword.free();
	skybox.free();

	vkDestroyDescriptorPool(Scope.GetDevice(), _descriptorPool, VK_NULL_HANDLE);
	std::erase_if(_presentFences, [&, this](VkFence& it) {
			vkDestroyFence(Scope.GetDevice(), it, VK_NULL_HANDLE);
			return true;
		});
	vkDestroySemaphore(Scope.GetDevice(), _swapchainSemaphore, VK_NULL_HANDLE);
	std::erase_if(_presentSemaphores, [&, this](VkSemaphore& it) {
			vkDestroySemaphore(Scope.GetDevice(), it, VK_NULL_HANDLE);
			return true;
		});
	Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).FreeCommandBuffers(_presentBuffers.size(), _presentBuffers.data());
	std::erase_if(_framebuffers, [&, this](VkFramebuffer& fb) {
			vkDestroyFramebuffer(Scope.GetDevice(), fb, VK_NULL_HANDLE);
			return true;
		});
	std::erase_if(_swapchainViews, [&, this](VkImageView& view) {
			vkDestroyImageView(Scope.GetDevice(), view, VK_NULL_HANDLE);
			return true;
		});
	std::erase_if(_depthAttachments, [&, this](std::unique_ptr<Image>& img) {
			img->Free();
			return true;
		});
	Scope.Destroy();
	vkDestroySurfaceKHR(_instance, _surface, VK_NULL_HANDLE);
	vkDestroyInstance(_instance, VK_NULL_HANDLE);
}

void VulkanBase::Step() const
{
	if (Scope.GetSwapchainExtent().width == 0 || Scope.GetSwapchainExtent().height == 0)
		return;

	uint32_t image_index = 0;
	vkAcquireNextImageKHR(Scope.GetDevice(), Scope.GetSwapchain(), UINT64_MAX, _swapchainSemaphore, VK_NULL_HANDLE, &image_index);

	vkWaitForFences(Scope.GetDevice(), 1, &_presentFences[image_index], VK_TRUE, UINT64_MAX);
	vkResetFences(Scope.GetDevice(), 1, &_presentFences[image_index]);

	_ubo->Update((void*)&camera.GetViewProjection());

	//////////////////////////////////////////////////////////
	{
		VkResult res;
		const VkCommandBuffer& cmd = _presentBuffers[image_index];
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		res = vkBeginCommandBuffer(cmd, &beginInfo);

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = _framebuffers[image_index];
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
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox.pipelineLayout, 0, 1, &skybox.descriptorSet, 0, VK_NULL_HANDLE);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox.pipeline);
		vkCmdDraw(cmd, 36, 1, 0, 0);

		VkDeviceSize offsets[] = { 0 };
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sword.pipelineLayout, 0, 1, &sword.descriptorSet, 0, VK_NULL_HANDLE);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sword.pipeline);
		vkCmdBindVertexBuffers(cmd, 0, 1, swordMesh->GetVertexBufferPointer(), offsets);
		vkCmdBindIndexBuffer(cmd, swordMesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, swordMesh->GetIndicesCount(), 1, 0, 0, 0);
		
		vkCmdEndRenderPass(cmd);

		vkEndCommandBuffer(cmd);
	}
	//////////////////////////////////////////////////////////

	VkSubmitInfo submitInfo{};
	std::array<VkSemaphore, 1> waitSemaphores = { _swapchainSemaphore };
	std::array<VkSemaphore, 1> signalSemaphores = { _presentSemaphores[image_index] };

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = waitSemaphores.size();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &_presentBuffers[image_index];
	submitInfo.signalSemaphoreCount = signalSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	VkResult res = vkQueueSubmit(Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetQueue(), 1, &submitInfo, _presentFences[image_index]);

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
	vkWaitForFences(Scope.GetDevice(), _presentFences.size(), _presentFences.data(), VK_TRUE, UINT64_MAX);

	std::erase_if(_framebuffers, [&, this](VkFramebuffer& fb) {
		vkDestroyFramebuffer(Scope.GetDevice(), fb, VK_NULL_HANDLE);
		return true;
	});
	std::erase_if(_depthAttachments, [&, this](std::unique_ptr<Image>& img) {
		img->Free();
		return true;
	});
	std::erase_if(_swapchainViews, [&, this](VkImageView& view) {
		vkDestroyImageView(Scope.GetDevice(), view, VK_NULL_HANDLE);
		return true;
	});
	_swapchainImages.resize(0);

	Scope.RecreateSwapchain(_surface);

	create_swapchain_images();
	create_framebuffers();

	glm::mat4 projection = glm::perspective(30.f, static_cast<float>(Scope.GetSwapchainExtent().width) / static_cast<float>(Scope.GetSwapchainExtent().height), 0.1f, 1000.f);
	projection[1][1] *= -1;
	camera.set_projection(projection);

	//it shouldn't change afaik, but better to keep it under control
	assert(_swapchainImages.size() == _presentBuffers.size());
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
	return vkCreateInstance(&createInfo, VK_NULL_HANDLE, &_instance) == VK_SUCCESS;
}

VkBool32 VulkanBase::create_swapchain_images()
{
	assert(Scope.GetSwapchain() != VK_NULL_HANDLE);

	uint32_t imagesCount = 0;
	vkGetSwapchainImagesKHR(Scope.GetDevice(), Scope.GetSwapchain(), &imagesCount, VK_NULL_HANDLE);
	_swapchainImages.resize(imagesCount);
	_depthAttachments.resize(imagesCount);
	VkBool32 res = vkGetSwapchainImagesKHR(Scope.GetDevice(), Scope.GetSwapchain(), &imagesCount, _swapchainImages.data()) == VK_SUCCESS;

	for (VkImage image : _swapchainImages) {
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
		_swapchainViews.push_back(imageView);
	}

	const std::array<uint32_t, 2> queueIndices = { Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex()};
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

	for (auto& attachment : _depthAttachments) {
		attachment = std::make_unique<Image>(Scope);
		attachment->CreateImage(depthInfo, allocCreateInfo)
				  .CreateImageView(viewInfo);
	}

	return res;
}

VkBool32 VulkanBase::create_framebuffers()
{
	assert(_swapchainImages.size() > 0 && Scope.GetRenderPass() != VK_NULL_HANDLE);

	_framebuffers.resize(_swapchainImages.size());

	VkBool32 res = 1;
	for (size_t i = 0; i < _framebuffers.size(); i++) {
		res = CreateFramebuffer(Scope.GetDevice(), Scope.GetRenderPass(), Scope.GetSwapchainExtent(), { _swapchainViews[i], _depthAttachments[i]->GetImageView() }, &_framebuffers[i]) & res;
	}

	return res;
}

VkBool32 VulkanBase::prepare_scene()
{
	VkBool32 res = 1;

	{
		auto vertAttributes = Vertex::getAttributeDescriptions();
		auto vertBindings = Vertex::getBindingDescription();

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.dstBinding = 0;
		write.pBufferInfo = &_ubo->GetDescriptor();
		VkDescriptorSetLayoutBinding binding{};
		binding.binding = 0;
		binding.descriptorCount = 1;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		vkShaderPipelineCreateInfo triCI{};
		triCI.bindingsCount = 1;
		triCI.pBindings = &binding;
		triCI.descriptorPool = _descriptorPool;
		triCI.renderPass = Scope.GetRenderPass();
		triCI.shaderStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		triCI.shaderName = "default";
		triCI.writesCount = 1;
		triCI.pWrites = &write;
		triCI.vertexInputBindingsCount = 1;
		triCI.vertexInputBindings = &vertBindings;
		triCI.vertexInputAttributesCount = vertAttributes.size();
		triCI.vertexInputAttributes = vertAttributes.data();

		res = sword.fill(Scope.GetDevice(), triCI) & res;
		swordMesh = fileManager->LoadMesh("content\\sword.fbx");
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////cube_texture

	{
		std::array<const char*, 6> sky_collection = { ("content\\Background_East.jpg"), ("content\\Background_West.jpg"),
			("content\\Background_Top.jpg"), ("content\\Background_Bottom.jpg"),
			("content\\Background_North.jpg"), ("content\\Background_South.jpg") };

		_skyboxImg = fileManager->LoadImages(sky_collection.data(), sky_collection.size(), VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

		VkCommandBuffer cmd;
		Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).AllocateCommandBuffers(1, &cmd);

		VkCommandBufferBeginInfo cmdBegin{};
		cmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &cmdBegin);

		VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = _skyboxImg->GetImage();
		barrier.subresourceRange = _skyboxImg->GetSubResourceRange();
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);


		GenerateMipMaps(Scope.GetDevice(), cmd, _skyboxImg->GetImage(), { _skyboxImg->GetExtent().width, _skyboxImg->GetExtent().height }, _skyboxImg->GetSubResourceRange());
		vkEndCommandBuffer(cmd);

		Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).Submit(cmd)
			.Wait()
			.FreeCommandBuffers(1, &cmd);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////skybox

	{
		std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
		bindings[0].binding = 0;
		bindings[0].descriptorCount = 1;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		bindings[1].binding = 1;
		bindings[1].descriptorCount = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		std::array<VkWriteDescriptorSet, 2> writes{};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].dstBinding = 0;
		writes[0].pBufferInfo = &_ubo->GetDescriptor();
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].dstBinding = 1;
		writes[1].pImageInfo = &_skyboxImg->GetDescriptor();
		
		vkShaderPipelineCreateInfo skyboxCI{};
		skyboxCI.bindingsCount = bindings.size();
		skyboxCI.pBindings = bindings.data();
		skyboxCI.writesCount = writes.size();
		skyboxCI.pWrites = writes.data();
		skyboxCI.renderPass = Scope.GetRenderPass();
		skyboxCI.descriptorPool = _descriptorPool;
		skyboxCI.shaderName = "background";
		skyboxCI.shaderStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		res = skybox.fill(Scope.GetDevice(), skyboxCI) & res;
	}

	return res;
}

std::vector<const char*> VulkanBase::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef VALIDATION
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	return extensions;
}

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

	if (createDebugUtilsMessengerEXT(_instance, &createInfo, VK_NULL_HANDLE, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

VkBool32 VulkanBase::checkValidationLayerSupport() const
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, VK_NULL_HANDLE);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) 
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