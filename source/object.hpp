#pragma once
#include "vulkan_objects/pipeline.hpp"
#include "vulkan_objects/descriptor_set.hpp"
#include "vulkan_objects/mesh.hpp"
#include "vulkan_objects/image.hpp"
#include <vector>
#include <array>
#include <string>

struct GraphicsObject
{
	GraphicsObject(const RenderScope& Scope)
		: pipeline(Scope), descriptorSet(Scope)
	{

	}

	GraphicsPipeline pipeline;
	DescriptorSet descriptorSet;

	//move to resource manager
	TAuto<Mesh> mesh;
	TVector<TAuto<Image>> textures;
};