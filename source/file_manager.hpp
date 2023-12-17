#pragma once
#include "pch.hpp"
#include "vulkan_objects/mesh.hpp"
#include "vulkan_objects/image.hpp"
#include "vulkan_objects/queue.hpp"
#include "vulkan_api.hpp"

class FileManager
{
public:
	FileManager(const RenderScope& Scope);

	~FileManager() {};

	FileManager(const FileManager& other) = delete;

	FileManager(const FileManager&& other) = delete;

	std::unique_ptr<Image> ImportImage(const char* path, const VkImageCreateFlags& flags = 0);

	std::unique_ptr<Image> ImportImages(const char** path, size_t numPaths, const VkImageCreateFlags& flags = 0);

	std::unique_ptr<Mesh> ImportMesh(const char* path);
private:
	std::unique_ptr<Image> create_image(void* pixels, int count, int w, int h, const VkImageCreateFlags& flags = 0);

	const RenderScope& Scope;
};