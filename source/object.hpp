#pragma once
#include <glfw/glfw3.h>
#include <vma/vk_mem_alloc.h>
#include <vector>
#include <array>
#include <string>

struct vkShaderPipelineCreateInfo
{
	std::string shaderName = "default";
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
	VkVertexInputBindingDescription* vertexInputBindings = VK_NULL_HANDLE;
	size_t vertexInputBindingsCount = 0;
	VkVertexInputAttributeDescription* vertexInputAttributes = VK_NULL_HANDLE;
	size_t vertexInputAttributesCount = 0;
};

struct vkShaderPipeline
{
	vkShaderPipeline() = default;
	vkShaderPipeline(const VkDevice& device, const vkShaderPipelineCreateInfo& objectCI);
	vkShaderPipeline(const vkShaderPipeline& other) = delete;
	void operator=(const vkShaderPipeline& other) = delete;
	vkShaderPipeline(vkShaderPipeline&& other) noexcept
		: _device(other._device), _pool(other._pool), pipeline(other.pipeline), pipelineLayout(other.pipelineLayout), 
		descriptorSet(other.descriptorSet), descriptorSetLayout(other.descriptorSetLayout)
	{
		other.pipeline = VK_NULL_HANDLE;
		other.pipelineLayout = VK_NULL_HANDLE;
		other.descriptorSet = VK_NULL_HANDLE;
		other.descriptorSetLayout = VK_NULL_HANDLE;
	}
	void operator=(vkShaderPipeline&& other) noexcept
	{
		if (pipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(_device, pipeline, VK_NULL_HANDLE);
		if (pipelineLayout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(_device, pipelineLayout, VK_NULL_HANDLE);
		if (_pool != VK_NULL_HANDLE)
			vkFreeDescriptorSets(_device, _pool, 1, &descriptorSet);
		if (descriptorSetLayout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(_device, descriptorSetLayout, VK_NULL_HANDLE);

		_device = other._device;
		_pool = other._pool;
		pipeline = other.pipeline;
		pipelineLayout = other.pipelineLayout;
		descriptorSet = other.descriptorSet;
		descriptorSetLayout = other.descriptorSetLayout;

		other.pipeline = VK_NULL_HANDLE;
		other.pipelineLayout = VK_NULL_HANDLE;
		other.descriptorSet = VK_NULL_HANDLE;
		other.descriptorSetLayout = VK_NULL_HANDLE;
	}
	~vkShaderPipeline();

	VkBool32 fill(const VkDevice& device, const vkShaderPipelineCreateInfo& objectCI);
	void free();

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
private:
	VkDevice _device = VK_NULL_HANDLE;
	VkDescriptorPool _pool = VK_NULL_HANDLE;
};