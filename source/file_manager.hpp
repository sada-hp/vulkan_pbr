#pragma once
#include "pch.hpp"
#include "vulkan_objects/mesh.hpp"
#include "vulkan_objects/image.hpp"
#include "vulkan_objects/queue.hpp"

class FileManager
{
public:
	FileManager(const RenderScope& Scope);

	std::unique_ptr<Image> LoadImage(const char* path, const VkImageCreateFlags& flags = 0);
	std::unique_ptr<Image> LoadImages(const char** path, size_t numPaths, const VkImageCreateFlags& flags = 0);
	std::unique_ptr<Mesh> LoadMesh(const char* path);
private:
	const RenderScope& Scope;
};