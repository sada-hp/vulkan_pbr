#pragma once
#include "vulkan_objects/pipeline.hpp"
#include "vulkan_objects/descriptor_set.hpp"

struct GraphicsObject
{
	GraphicsObject() {};
private:
	friend class VulkanBase;

	TAuto<Pipeline> pipeline;
	TAuto<DescriptorSet> descriptorSet;
};