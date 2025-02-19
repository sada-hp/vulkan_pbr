#pragma once
#include <glfw/glfw3.h>
#include <vector>
#include <array>
#include <string>
#include "Vulkan/buffer.hpp"
#include "Vulkan/scope.hpp"
#include <glm/gtx/hash.hpp>

struct MeshVertex
{
	static const VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(MeshVertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static const std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(MeshVertex, position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(MeshVertex, normal);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(MeshVertex, tangent);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(MeshVertex, uv);

		return attributeDescriptions;
	}

	bool operator==(const MeshVertex& other) const
	{
		return position == other.position
			&& normal == other.normal
			&& tangent == other.tangent
			&& uv == other.uv
			&& submesh == other.submesh;
	}

	uint32_t submesh = 0;
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 uv;
};

struct TerrainVertex
{
	static const VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(TerrainVertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static const std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(TerrainVertex, position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(TerrainVertex, uv);

		return attributeDescriptions;
	}

	bool operator==(const TerrainVertex& other) const
	{
		return position == other.position && uv == other.uv;
	}

	glm::vec4 position;
	glm::vec4 uv;
};

template<>
struct std::hash<MeshVertex>
{
	size_t operator()(MeshVertex const& vertex) const
	{
		return ((std::hash<glm::vec3>()(vertex.position)
			^ (std::hash<glm::vec3>()(vertex.normal))
			^ (std::hash<glm::vec3>()(vertex.tangent))
			^ (std::hash<glm::vec2>()(vertex.uv))
			^ (std::hash<uint32_t>()(vertex.submesh))) >> 1);
	}
};

template<>
struct std::hash<TerrainVertex>
{
	size_t operator()(TerrainVertex const& vertex) const
	{
		return (((std::hash<glm::vec4>()(vertex.position) ^ (std::hash<glm::vec4>()(vertex.uv))) >> 1));
	}
};

struct VulkanMesh
{
	VulkanMesh(const RenderScope& Scope, MeshVertex* vertices, size_t numVertices, uint32_t* indices, size_t numIndices);

	VulkanMesh(const RenderScope& Scope, TerrainVertex* vertices, size_t numVertices, uint32_t* indices, size_t numIndices);

	VulkanMesh(const VulkanMesh& other) = delete;

	void operator=(const VulkanMesh& other) = delete;

	VulkanMesh(VulkanMesh&& other) noexcept
		: Scope(other.Scope), vertexBuffer(std::move(other.vertexBuffer)), indexBuffer(std::move(other.indexBuffer)),
		indicesCount(other.indicesCount), verticesCount(other.verticesCount)
	{
		other.indicesCount = 0;
		other.verticesCount = 0;
	}

	void operator =(VulkanMesh&& other) noexcept {
		Scope = other.Scope;
		vertexBuffer = std::move(other.vertexBuffer);
		indexBuffer = std::move(other.indexBuffer);
		indicesCount = other.indicesCount;
		verticesCount = other.indicesCount;

		other.indicesCount = 0;
		other.verticesCount = 0;
	}

	~VulkanMesh() = default;

	std::shared_ptr<const Buffer> GetVertexBuffer() const { return vertexBuffer; };

	std::shared_ptr<const Buffer> GetIndexBuffer() const { return indexBuffer; };

	uint32_t GetIndicesCount() const { return indicesCount; };

	uint32_t GetVerticesCount() const { return verticesCount; };

private:
	std::shared_ptr<Buffer> vertexBuffer = {};
	std::shared_ptr<Buffer> indexBuffer = {};
	uint32_t indicesCount = 0;
	uint32_t verticesCount = 0;

	const RenderScope* Scope = VK_NULL_HANDLE;
};