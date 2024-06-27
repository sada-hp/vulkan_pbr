#pragma once
#include "pch.hpp"
#include "vulkan_objects/mesh.hpp"
#include "vulkan_objects/image.hpp"
#include "vulkan_objects/queue.hpp"
#include "vulkan_api.hpp"

namespace GRVkFile
{
	TAuto<VulkanImage> _importImage(const RenderScope& Scope, const char* path, const VkFormat& format, const VkImageCreateFlags& flags = 0);

	TAuto<Mesh> _importMesh(const RenderScope& Scope, const char* path);
};