#pragma once
#include "pch.hpp"
#include "vulkan_objects/mesh.hpp"
#include "vulkan_objects/image.hpp"
#include "vulkan_objects/queue.hpp"
#include "vulkan_api.hpp"

namespace GRFile
{
	TAuto<Image> ImportImage(const RenderScope& Scope, const char* path, const VkImageCreateFlags& flags = 0);

	TAuto<Image> ImportImages(const RenderScope& Scope, const char** path, size_t numPaths, const VkImageCreateFlags& flags = 0);

	TAuto<Mesh> ImportMesh(const RenderScope& Scope, const char* path);
};