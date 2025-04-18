#include "pch.hpp"
#include "mesh.hpp"

VulkanMesh::VulkanMesh(const RenderScope& InScope, MeshVertex* vertices, size_t numVertices, uint32_t* indices, size_t numIndices)
	: Scope(&InScope)
{
	VkBufferCreateInfo sbInfo{};
	sbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	sbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sbInfo.size = sizeof(MeshVertex) * numVertices;

	VmaAllocationCreateInfo sbAlloc{};
	sbAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	vertexBuffer = std::make_unique<Buffer>(*Scope, sbInfo, sbAlloc);

	sbInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	sbInfo.size = sizeof(uint32_t) * numIndices;
	indexBuffer = std::make_unique<Buffer>(*Scope, sbInfo, sbAlloc);

	vertexBuffer->Update(vertices);
	indexBuffer->Update(indices);

	verticesCount = numVertices;
	indicesCount = numIndices;
}

VulkanMesh::VulkanMesh(const RenderScope& InScope, TerrainVertex* vertices, size_t numVertices, uint32_t* indices, size_t numIndices)
	: Scope(&InScope)
{
	std::vector<uint32_t> queueFamilies = FindDeviceQueues(InScope.GetPhysicalDevice(), { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT });
	std::sort(queueFamilies.begin(), queueFamilies.end());
	queueFamilies.resize(std::distance(queueFamilies.begin(), std::unique(queueFamilies.begin(), queueFamilies.end())));

	VkBufferCreateInfo sbInfo{};
	sbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sbInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	sbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sbInfo.queueFamilyIndexCount = queueFamilies.size();
	sbInfo.pQueueFamilyIndices = queueFamilies.data();
	sbInfo.size = sizeof(TerrainVertex) * numVertices;

	VmaAllocationCreateInfo sbAlloc{};
	sbAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	vertexBuffer = std::make_unique<Buffer>(*Scope, sbInfo, sbAlloc);

	sbInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	sbInfo.size = sizeof(uint32_t) * numIndices;
	indexBuffer = std::make_unique<Buffer>(*Scope, sbInfo, sbAlloc);

	vertexBuffer->Update(vertices);
	indexBuffer->Update(indices);

	verticesCount = numVertices;
	indicesCount = numIndices;
}