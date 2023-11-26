#include "pch.hpp"
#include "mesh.hpp"

Mesh::Mesh(const RenderScope& InScope, Vertex* vertices, size_t numVertices, uint32_t* indices, size_t numIndices)
	: Scope(&InScope)
{
	VkBufferCreateInfo sbInfo{};
	sbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	sbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sbInfo.size = sizeof(Vertex) * numVertices;
	VmaAllocationCreateInfo sbAlloc{};
	sbAlloc.usage = VMA_MEMORY_USAGE_AUTO;
	sbAlloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	vertexBuffer = std::make_unique<Buffer>(*Scope, sbInfo, sbAlloc);

	sbInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	sbInfo.size = sizeof(uint32_t) * numIndices;
	indexBuffer = std::make_unique<Buffer>(*Scope, sbInfo, sbAlloc);

	vertexBuffer->Map().Update(vertices).UnMap();
	indexBuffer->Map().Update(indices).UnMap();

	verticesCount = numVertices;
	indicesCount = numIndices;
}
