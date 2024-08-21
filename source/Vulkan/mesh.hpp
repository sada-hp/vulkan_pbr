#pragma once
#include <glfw/glfw3.h>
#include <vector>
#include <array>
#include <string>
#include "Vulkan/buffer.hpp"
#include "Vulkan/vertex.hpp"
#include "Vulkan/scope.hpp"

struct Mesh
{
	Mesh(const RenderScope& Scope, Vertex* vertices, size_t numVertices, uint32_t* indices, size_t numIndices);

	Mesh(const Mesh& other) = delete;

	void operator=(const Mesh& other) = delete;

	Mesh(Mesh&& other) noexcept
		: Scope(other.Scope), vertexBuffer(std::move(other.vertexBuffer)), indexBuffer(std::move(other.indexBuffer)),
		indicesCount(other.indicesCount), verticesCount(other.verticesCount)
	{
		other.indicesCount = 0;
		other.verticesCount = 0;
	}

	void operator =(Mesh&& other) noexcept {
		Scope = other.Scope;
		vertexBuffer = std::move(other.vertexBuffer);
		indexBuffer = std::move(other.indexBuffer);
		indicesCount = other.indicesCount;
		verticesCount = other.indicesCount;

		other.indicesCount = 0;
		other.verticesCount = 0;
	}

	~Mesh() = default;

	TShared<const Buffer> GetVertexBuffer() const { return vertexBuffer; };

	TShared<const Buffer> GetIndexBuffer() const { return indexBuffer; };

	uint32_t GetIndicesCount() const { return indicesCount; };

	uint32_t GetVerticesCount() const { return verticesCount; };

private:
	TShared<Buffer> vertexBuffer = {};
	TShared<Buffer> indexBuffer = {};
	uint32_t indicesCount = 0;
	uint32_t verticesCount = 0;

	const RenderScope* Scope = VK_NULL_HANDLE;
};