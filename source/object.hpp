#pragma once
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/hash.hpp>
#include <vma/vk_mem_alloc.h>
#include <stb/stb_image.h>
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

struct vkQueueStruct
{
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t index = 0;
};

struct vkImageStruct
{
	VkBool32 create(const VkDevice& device, const VmaAllocator& allocator, const VkImageCreateInfo& imgInfo, const VmaAllocationCreateInfo& allocCreateInfo, VkImageViewCreateInfo* viewInfo = VK_NULL_HANDLE, VkSamplerCreateInfo* samplerInfo = VK_NULL_HANDLE);
	void destroy(const VkDevice& device, const VmaAllocator& allocator);

	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VmaAllocation memory = VK_NULL_HANDLE;
	VmaAllocationInfo allocInfo = {};
	VkDescriptorImageInfo descriptorInfo = {};
};

struct vkBufferStruct
{
	VkBool32 create(const VmaAllocator& allocator, const VkBufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo);
	void destroy(const VmaAllocator& allocator);

	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation memory = VK_NULL_HANDLE;
	VmaAllocationInfo allocInfo = {};
	VkDescriptorBufferInfo descriptorInfo = {};
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

struct vkVertex
{
	static const VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(vkVertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static const std::array<VkVertexInputAttributeDescription, 1> getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(vkVertex, position);

		return attributeDescriptions;
	}

	bool operator==(const vkVertex& other) const
	{
		return position == other.position && meshIndex == other.meshIndex;
	}

	uint32_t meshIndex = 0;
	glm::vec3 position;
};

template<>
struct std::hash<vkVertex>
{
	size_t operator()(vkVertex const& vertex) const
	{
		return ((std::hash<glm::vec3>()(vertex.position) ^ (std::hash<uint32_t>()(vertex.meshIndex))) >> 1);
	}
};

struct vkMesh
{
	void loadMesh(const VmaAllocator& allocator, const char* path);
	void destroy(const VmaAllocator& allocator);

	vkBufferStruct vertexBuffer = {};
	vkBufferStruct indexBuffer = {};
	int indicesCount = 0;
	int verticesCount = 0;
};