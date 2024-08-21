#pragma once
#include "Vulkan/pipeline.hpp"
#include "Vulkan/descriptor_set.hpp"

struct GraphicsObject
{
	GraphicsObject() {};
private:
	friend class VulkanBase;

	TAuto<Pipeline> pipeline;
	TAuto<DescriptorSet> descriptorSet;
};