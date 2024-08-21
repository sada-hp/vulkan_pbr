#pragma once
#include "pch.hpp"
#include "Vulkan/mesh.hpp"
#include "Vulkan/image.hpp"
#include "Vulkan/queue.hpp"
#include "Vulkan/vulkan_api.hpp"

namespace GRVkFile
{
	TAuto<VulkanImage> _importImage(const RenderScope& Scope, const char* path, const VkFormat& format, const VkImageCreateFlags& flags = 0);

	TAuto<Mesh> _importMesh(const RenderScope& Scope, const char* path);
};