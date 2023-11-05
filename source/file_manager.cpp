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

std::unique_ptr<Image> FileManager::LoadImage(const char* path, const VkImageCreateFlags& flags)
{
	return VK_NULL_HANDLE;
}

std::unique_ptr<Image> FileManager::LoadImages(const char** path, size_t numPaths, const VkImageCreateFlags& flags)
{
	assert(numPaths > 0 && path != nullptr);

	int w, h, c;
	stbi_info((exec_path + path[0]).c_str(), &w, &h, &c);
	int resolution = w * h * 4;
	unsigned char* pixels = (unsigned char*)malloc(resolution * numPaths);
	std::vector<std::future<void>> loaders(numPaths);

	for (int i = 0; i < numPaths; i++) {
		loaders[i] = std::async(std::launch::async, [&, i]() {
				int x, y, c;
				void* target = stbi_load((exec_path + path[i]).c_str(), &x, &y, &c, 4);
				memcpy(pixels + resolution * i, target, resolution);
				free(target);
			});
	}
	std::for_each(loaders.begin(), loaders.end(), [](const auto& it) {it.wait(); });

	uint32_t familyIndex = Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex();
	VkBufferCreateInfo sbInfo{};
	sbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	sbInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	sbInfo.queueFamilyIndexCount = 1;
	sbInfo.pQueueFamilyIndices = &familyIndex;
	sbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sbInfo.size = resolution * numPaths;
	VmaAllocationCreateInfo sbAlloc{};
	sbAlloc.usage = VMA_MEMORY_USAGE_AUTO;
	sbAlloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	Buffer stagingBuffer(Scope, sbInfo, sbAlloc);
	stagingBuffer.Update(pixels);

	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;
	VkImageSubresourceRange skySubRes{};
	skySubRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	skySubRes.baseArrayLayer = 0;
	skySubRes.baseMipLevel = 0;
	skySubRes.layerCount = 6;
	skySubRes.levelCount = mipLevels;
	VkImageCreateInfo skyInfo{};
	skyInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	skyInfo.arrayLayers = numPaths;
	skyInfo.extent = { (uint32_t)w, (uint32_t)h, 1u };
	skyInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	skyInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	skyInfo.mipLevels = mipLevels;
	skyInfo.flags = flags;
	skyInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	skyInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	skyInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	skyInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	skyInfo.imageType = VK_IMAGE_TYPE_2D;

	VkImageViewCreateInfo skyView{};
	skyView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	skyView.format = VK_FORMAT_R8G8B8A8_SRGB;
	skyView.viewType = (flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	skyView.subresourceRange = skySubRes;

	VmaAllocationCreateInfo skyAlloc{};
	skyAlloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(Scope.GetPhysicalDevice(), &properties);
	std::unique_ptr<Image> target = std::make_unique<Image>(Scope);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1;

	target->CreateImage(skyInfo, skyAlloc)
		.CreateImageView(skyView)
		.CreateSampler(samplerInfo);

	VkCommandBuffer cmd;
	Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).AllocateCommandBuffers(1, &cmd);

	VkCommandBufferBeginInfo cmdBegin{};
	cmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &cmdBegin);

	VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = target->GetImage();
	barrier.subresourceRange = skySubRes;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

	int i = 0;
	std::vector<VkBufferImageCopy> regions(numPaths);
	for (auto& region : regions) {
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.baseArrayLayer = i;
		region.imageSubresource.layerCount = 1u;
		region.imageSubresource.mipLevel = 0;
		region.bufferOffset = i * resolution;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { (uint32_t)w, (uint32_t)h, 1u };
		i++;
	}

	vkCmdCopyBufferToImage(cmd, stagingBuffer.GetBuffer(), target->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size(), regions.data());
	vkEndCommandBuffer(cmd);
	
	Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);
	
	free(pixels);

	return target;
}

std::unique_ptr<Mesh> FileManager::LoadMesh(const char* path)
{
	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	Assimp::Importer importer;
	std::string file = exec_path + path;
	const aiScene* model = importer.ReadFile(file, aiProcess_Triangulate | aiProcess_FlipUVs);
	auto err = importer.GetErrorString();

	assert(model != VK_NULL_HANDLE);

	for (int mesh_ind = 0; mesh_ind < model->mNumMeshes; mesh_ind++) {
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