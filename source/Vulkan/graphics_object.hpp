#pragma once
#include "Vulkan/mesh.hpp"
#include "Vulkan/pipeline.hpp"
#include "Vulkan/descriptor_set.hpp"

struct GraphicsObject
{
	GraphicsObject() {};
private:
	friend class VulkanBase;

	std::unique_ptr<Pipeline> pipeline;
	std::unique_ptr<DescriptorSet> descriptorSet;
};

struct PBRConstants
{
	glm::mat4 World;
	glm::vec4 Color;
	float RoughnessMultiplier = 1.0;
	float Metallic = 0.0;
	float HeightScale = 1.0;
};

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