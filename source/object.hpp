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
	vkImageStruct() = default;
	vkImageStruct(const VkDevice& device, const VmaAllocator& allocator, const VkImageCreateInfo& imgInfo, const VmaAllocationCreateInfo& allocCreateInfo, VkImageViewCreateInfo* viewInfo = VK_NULL_HANDLE, VkSamplerCreateInfo* samplerInfo = VK_NULL_HANDLE);
	vkImageStruct(const vkImageStruct& other) = delete;
	void operator=(const vkImageStruct& other) = delete;
	vkImageStruct(vkImageStruct&& other) noexcept
		: _device(other._device), _allocator(other._allocator), image(other.image), view(other.view), sampler(other.sampler), memory(other.memory),
		allocInfo(std::move(other.allocInfo)), descriptorInfo(std::move(other.descriptorInfo))
	{
		other.image = VK_NULL_HANDLE;
		other.view = VK_NULL_HANDLE;
		other.sampler = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}
	void operator=(vkImageStruct&& other) noexcept
	{
		if (sampler != VK_NULL_HANDLE)
			vkDestroySampler(_device, sampler, VK_NULL_HANDLE);
		if (view != VK_NULL_HANDLE)
			vkDestroyImageView(_device, view, VK_NULL_HANDLE);
		if (image != VK_NULL_HANDLE)
			vmaDestroyImage(_allocator, image, memory);

		_device = other._device;
		_allocator = other._allocator;
		image = other.image;
		view = other.view;
		sampler = other.sampler;
		memory = other.memory;
		allocInfo = std::move(other.allocInfo);
		descriptorInfo = std::move(other.descriptorInfo);

		other.image = VK_NULL_HANDLE;
		other.view = VK_NULL_HANDLE;
		other.sampler = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}
	~vkImageStruct();

	VkBool32 fill(const VkDevice& device, const VmaAllocator& allocator, const VkImageCreateInfo& imgInfo, const VmaAllocationCreateInfo& allocCreateInfo, VkImageViewCreateInfo* viewInfo = VK_NULL_HANDLE, VkSamplerCreateInfo* samplerInfo = VK_NULL_HANDLE);
	void free();

	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VmaAllocation memory = VK_NULL_HANDLE;
	VmaAllocationInfo allocInfo = {};
	VkDescriptorImageInfo descriptorInfo = {};
private:
	VkDevice _device = VK_NULL_HANDLE;
	VmaAllocator _allocator = VK_NULL_HANDLE;
};

struct vkBufferStruct
{
	vkBufferStruct() = default;
	vkBufferStruct(const VmaAllocator& allocator, const VkBufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo);
	vkBufferStruct(const vkBufferStruct& other) = delete;
	void operator=(const vkBufferStruct& other) = delete;
	vkBufferStruct(vkBufferStruct&& other) noexcept
		: _allocator(other._allocator), buffer(other.buffer), memory(other.memory), allocInfo(std::move(allocInfo)), descriptorInfo(std::move(other.descriptorInfo))
	{
		other.buffer = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}
	void operator=(vkBufferStruct&& other) noexcept
	{
		if (buffer != VK_NULL_HANDLE)
			vmaDestroyBuffer(_allocator, buffer, memory);

		_allocator = other._allocator;
		buffer = other.buffer;
		memory = other.memory;
		allocInfo = std::move(other.allocInfo);
		descriptorInfo = std::move(other.descriptorInfo);

		other.buffer = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.allocInfo = {};
		other.descriptorInfo = {};
	}
	~vkBufferStruct();

	VkBool32 fill(const VmaAllocator& allocator, const VkBufferCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo);
	void free();

	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation memory = VK_NULL_HANDLE;
	VmaAllocationInfo allocInfo = {};
	VkDescriptorBufferInfo descriptorInfo = {};
private:
	VmaAllocator _allocator = VK_NULL_HANDLE;
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
	vkMesh() = default;
	vkMesh(const VmaAllocator& allocator, const char* path);
	vkMesh(const vkMesh& other) = delete;
	void operator=(const vkMesh& other) = delete;
	vkMesh(vkMesh&& other) noexcept
		: _allocator(other._allocator), vertexBuffer(std::move(other.vertexBuffer)), indexBuffer(std::move(other.indexBuffer)),
		indicesCount(other.indicesCount), verticesCount(other.verticesCount)
	{
		other.indicesCount = 0;
		other.verticesCount = 0;
	}
	void operator =(vkMesh&& other) noexcept
	{
		_allocator = other._allocator;
		vertexBuffer = std::move(other.vertexBuffer);
		indexBuffer = std::move(other.indexBuffer);
		indicesCount = other.indicesCount;
		verticesCount = other.verticesCount;
	}
	~vkMesh() = default;

	VkBool32 fill(const VmaAllocator& allocator, const char* path);
	void free();

	vkBufferStruct vertexBuffer = {};
	vkBufferStruct indexBuffer = {};
	uint32_t indicesCount = 0;
	uint32_t verticesCount = 0;
private:
	VmaAllocator _allocator = VK_NULL_HANDLE;
};