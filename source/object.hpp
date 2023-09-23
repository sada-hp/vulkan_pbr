#pragma once
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vma/vk_mem_alloc.h>
#include <stb/stb_image.h>
#include <vector>
#include <array>
#include <string>

struct vkShaderPipelineCreateInfo
{
	std::string shaderName = "";
	uint32_t shaderStages;
	VkDescriptorSetLayoutBinding* pBindings = VK_NULL_HANDLE;
	size_t bindingsCount = 0;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	uint32_t subpass = 0;
	VkWriteDescriptorSet* pWrites = VK_NULL_HANDLE;
	size_t writesCount = 0;
	VkPushConstantRange* pPushContants = VK_NULL_HANDLE;
	size_t pushConstantsCount = 0;
};

struct vkShaderPipeline
{
	VkBool32 create(const VkDevice& device, const vkShaderPipelineCreateInfo& objectCI);

	void destroy(const VkDevice& device, const VkDescriptorPool& pool = VK_NULL_HANDLE);

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
};