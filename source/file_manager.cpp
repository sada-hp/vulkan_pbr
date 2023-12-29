#include "pch.hpp"
#include "file_manager.hpp"
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

extern std::string exec_path;

FileManager::FileManager(const RenderScope& InScope)
	: Scope(InScope)
{

}

std::unique_ptr<Image> FileManager::ImportImage(const char* path, const VkImageCreateFlags& flags)
{
	assert(path != nullptr);

	int w, h, c;
	unsigned char* pixels = stbi_load((exec_path + path).c_str(), &w, &h, &c, 4);
	std::unique_ptr<Image> target = create_image(pixels, 1, w, h, flags);
	free(pixels);

	return target;
}

std::unique_ptr<Image> FileManager::ImportImages(const char** path, size_t numPaths, const VkImageCreateFlags& flags)
{
	assert(numPaths > 0 && path != nullptr);

	int w, h, c;
	stbi_info((exec_path + path[0]).c_str(), &w, &h, &c);
	int resolution = w * h * 4;
	unsigned char* pixels = (unsigned char*)malloc(resolution * numPaths);
	std::vector<std::future<void>> loaders(numPaths);

	for (int i = 0; i < numPaths; i++) {
		loaders[i] = std::async(std::launch::async, [=]() {
				int x, y, c;
				void* target = stbi_load((exec_path + path[i]).c_str(), &x, &y, &c, 4);
				memcpy(pixels + resolution * i, target, resolution);
				free(target);
			});
	}
	std::for_each(loaders.begin(), loaders.end(), [](const auto& it) {it.wait(); });

	std::unique_ptr<Image> target = create_image(pixels, numPaths, w, h, flags);
	free(pixels);

	return target;
}

std::unique_ptr<Mesh> FileManager::ImportMesh(const char* path)
{
	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	Assimp::Importer importer;
	std::string file = exec_path + path;
	const aiScene* model = importer.ReadFile(file, aiProcess_Triangulate | aiProcess_FlipUVs);
	auto err = importer.GetErrorString();

	assert(model != VK_NULL_HANDLE);

	for (uint32_t mesh_ind = 0; mesh_ind < model->mNumMeshes; mesh_ind++) {
		auto num_vert = model->mMeshes[mesh_ind]->mNumVertices;
		auto cur_mesh = model->mMeshes[mesh_ind];
		auto name3 = model->mMeshes[mesh_ind]->mName;
		auto uv_ind = mesh_ind;

		for (int vert_ind = 0; vert_ind < num_vert; vert_ind++) {
			Vertex vertex{
				mesh_ind,
				{cur_mesh->mVertices[vert_ind].x, -cur_mesh->mVertices[vert_ind].y, cur_mesh->mVertices[vert_ind].z}
			};

			if (uniqueVertices.count(vertex) == 0) {
				int index = uniqueVertices.size();
				uniqueVertices[vertex] = index;

				indices.push_back(index);
				vertices.push_back(std::move(vertex));
			}
			else {
				indices.push_back(uniqueVertices[vertex]);
			}
		}
	}

	return std::make_unique<Mesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
}

std::unique_ptr<Image> FileManager::create_image(void* pixels, int count, int w, int h, const VkImageCreateFlags& flags)
{
	assert(pixels != nullptr && count > 0 && w > 0 && h > 0);

	int resolution = w * h * 4;
	uint32_t familyIndex = Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex();
	VkBufferCreateInfo sbInfo{};
	sbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sbInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	sbInfo.queueFamilyIndexCount = 1;
	sbInfo.pQueueFamilyIndices = &familyIndex;
	sbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sbInfo.size = resolution * count;
	VmaAllocationCreateInfo sbAlloc{};
	sbAlloc.usage = VMA_MEMORY_USAGE_AUTO;
	sbAlloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	Buffer stagingBuffer(Scope, sbInfo, sbAlloc);
	stagingBuffer.Update(pixels);

	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;
	VkImageSubresourceRange subRes{};
	subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subRes.baseArrayLayer = 0;
	subRes.baseMipLevel = 0;
	subRes.layerCount = count;
	subRes.levelCount = mipLevels;
	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.arrayLayers = count;
	imageCI.extent = { (uint32_t)w, (uint32_t)h, 1u };
	imageCI.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCI.mipLevels = mipLevels;
	imageCI.flags = flags;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageCI.imageType = VK_IMAGE_TYPE_2D;

	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageViewCI.viewType = (flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.subresourceRange = subRes;

	VmaAllocationCreateInfo skyAlloc{};
	skyAlloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(Scope.GetPhysicalDevice(), &properties);
	std::unique_ptr<Image> target = std::make_unique<Image>(Scope);
	target->CreateImage(imageCI, skyAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(SamplerFlagBits::RepeatUVW | SamplerFlagBits::AnisatropyEnabled);

	VkCommandBuffer cmd;
	Scope.GetQueue(VK_QUEUE_TRANSFER_BIT)
		.AllocateCommandBuffers(1, &cmd);

	BeginOneTimeSubmitCmd(cmd);
	target->TransitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(cmd, target->GetImage(), stagingBuffer.GetBuffer(), subRes, { (uint32_t)w, (uint32_t)h, 1u });
	EndCommandBuffer(cmd);

	Scope.GetQueue(VK_QUEUE_TRANSFER_BIT)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers(1, &cmd);

	BeginOneTimeSubmitCmd(cmd);
	target->GenerateMipMaps(cmd);
	EndCommandBuffer(cmd);

	Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	return target;
}