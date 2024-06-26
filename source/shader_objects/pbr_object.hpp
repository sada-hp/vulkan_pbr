#pragma once
#include "general_object.hpp"
#include "vulkan_objects/mesh.hpp"
#include "vulkan_objects/image.hpp"

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

private:
	friend class VulkanBase;

	TAuto<Mesh> mesh;
	bool dirty = false;
};