#pragma once
#include "Vulkan/mesh.hpp"
#include "Vulkan/pipeline.hpp"
#include "Vulkan/descriptor_set.hpp"

struct GraphicsObject
{
	GraphicsObject() {};
private:
	friend class VulkanBase;

	std::unique_ptr<GraphicsPipeline> pipeline;
	std::unique_ptr<DescriptorSet> descriptorSet;
};

#pragma pack(push, 1)
struct PBRConstants
{
	glm::dvec3 Offset;
	double al1 = 42.0; // alignment
	glm::mat3x4 Orientation;
	
	glm::vec4 Color;
	float RoughnessMultiplier = 1.0;
	float Metallic = 0.0;
	float HeightScale = 1.0;

	static size_t VertexSize()
	{
		return offsetof(PBRConstants, Orientation) + sizeof(PBRConstants::Orientation);
	}

	static size_t FragmentSize()
	{
		return sizeof(PBRConstants) - VertexSize();
	}
};
#pragma pack(pop)

struct PBRObject : public GraphicsObject
{
	PBRObject()
	{

	};

	bool is_dirty() { return dirty; }

private:
	friend class VulkanBase;

	std::unique_ptr<VulkanMesh> mesh;
	bool dirty = false;
};