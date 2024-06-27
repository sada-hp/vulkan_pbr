#include "pch.hpp"
#include "file_manager.hpp"
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

extern std::string exec_path;

void calculate_normals(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	for (int i = 0; i < indices.size(); i += 3)
	{
		Vertex& A = vertices[indices[i]];
		Vertex& B = vertices[indices[i + 1]];
		Vertex& C = vertices[indices[i + 2]];
		glm::vec3 contributingNormal = glm::cross(glm::vec3(B.position) - glm::vec3(A.position), glm::vec3(C.position) - glm::vec3(A.position));
		float area = glm::length(contributingNormal) / 2.f;
		glm::vec3 contributingNormal2 = glm::normalize(contributingNormal) * area;

		vertices[indices[i]].normal += contributingNormal;
		vertices[indices[i + 1]].normal += contributingNormal;
		vertices[indices[i + 2]].normal += contributingNormal;
	}

	for (int i = 0; i < vertices.size(); i++)
	{
		vertices[i].normal = glm::normalize(vertices[i].normal);
	}
}

void calculate_tangents(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, float u_scale, float v_scale)
{
	for (int i = 0; i < indices.size(); i += 3)
	{
		Vertex& A = vertices[indices[i]];
		Vertex& B = vertices[indices[i + 1]];
		Vertex& C = vertices[indices[i + 2]];

		glm::vec2 uv0 = (A.uv - 0.5f) * u_scale + (0.5f * v_scale);
		glm::vec2 uv1 = (B.uv - 0.5f) * u_scale + (0.5f * v_scale);
		glm::vec2 uv2 = (C.uv - 0.5f) * u_scale + (0.5f * v_scale);

		glm::vec3 diff1;
		glm::vec3 diff2;
		glm::vec2 delta1;
		glm::vec2 delta2;

		diff1 = B.position - A.position;
		diff2 = C.position - A.position;
		delta1 = glm::vec2(uv1.x, 1.f - uv1.y) - glm::vec2(uv0.x, 1.f - uv0.y);
		delta2 = glm::vec2(uv2.x, 1.f - uv2.y) - glm::vec2(uv0.x, 1.f - uv0.y);

		float f = 1.0f / (delta1.x * delta2.y - delta1.y * delta2.x);
		glm::vec3 tangent;
		glm::vec3 bitangent;

		tangent.x = f * (delta2.y * diff1.x - delta1.y * diff2.x);
		tangent.y = f * (delta2.y * diff1.y - delta1.y * diff2.y);
		tangent.z = f * (delta2.y * diff1.z - delta1.y * diff2.z);

		vertices[indices[i]].tangent = tangent;
		vertices[indices[i + 1]].tangent = tangent;
		vertices[indices[i + 2]].tangent = tangent;
	}
}

TAuto<VulkanImage> create_image(const RenderScope& Scope, void* pixels, int count, int w, int h, const VkFormat& format, const VkImageCreateFlags& flags)
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
	imageCI.format = format;
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
	imageViewCI.format = imageCI.format;
	imageViewCI.viewType = (flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.subresourceRange = subRes;

	VmaAllocationCreateInfo skyAlloc{};
	skyAlloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(Scope.GetPhysicalDevice(), &properties);
	std::unique_ptr<VulkanImage> target = std::make_unique<VulkanImage>(Scope);
	target->CreateImage(imageCI, skyAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::BillinearRepeat);

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

TAuto<VulkanImage> GRVkFile::_importImage(const RenderScope& Scope, const char* path, const VkFormat& format, const VkImageCreateFlags& flags)
{
	assert(path != nullptr);

	if (!path)
		return VK_NULL_HANDLE;

	int w, h, c;
	unsigned char* pixels = stbi_load((exec_path + path).c_str(), &w, &h, &c, 4);
	TAuto<VulkanImage> target = create_image(Scope, pixels, 1, w, h, format, flags);
	free(pixels);

	return target;
}

TAuto<Mesh> GRVkFile::_importMesh(const RenderScope& Scope, const char* path)
{
	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	TVector<uint32_t> indices;
	TVector<Vertex> vertices;
	Assimp::Importer importer;
	std::string file = exec_path + path;
	const aiScene* model = importer.ReadFile(file, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_FixInfacingNormals);
	auto err = importer.GetErrorString();

	assert(model != VK_NULL_HANDLE);

	if (!model)
		return VK_NULL_HANDLE;

	for (uint32_t submesh_ind = 0; submesh_ind < model->mNumMeshes; submesh_ind++) 
	{
		auto num_vert = model->mMeshes[submesh_ind]->mNumVertices;
		auto cur_mesh = model->mMeshes[submesh_ind];
		auto name3 = model->mMeshes[submesh_ind]->mName;
		auto uv_ind = submesh_ind;

		for (int vert_ind = 0; vert_ind < num_vert; vert_ind++) 
		{
			Vertex vertex{};
			vertex.position = { cur_mesh->mVertices[vert_ind].x, cur_mesh->mVertices[vert_ind].y, cur_mesh->mVertices[vert_ind].z };
			vertex.uv = { cur_mesh->mTextureCoords[uv_ind][vert_ind].x, 1.0 - cur_mesh->mTextureCoords[uv_ind][vert_ind].y };
			vertex.submesh = submesh_ind;

			if (cur_mesh->HasNormals())
				vertex.normal = { cur_mesh->mNormals[vert_ind].x, cur_mesh->mNormals[vert_ind].y, cur_mesh->mNormals[vert_ind].z };
			
			if (cur_mesh->HasTangentsAndBitangents())
				vertex.tangent = { cur_mesh->mTangents[vert_ind].x, cur_mesh->mTangents[vert_ind].y, cur_mesh->mTangents[vert_ind].z };

			if (uniqueVertices.count(vertex) == 0) 
			{
				int index = uniqueVertices.size();
				uniqueVertices[vertex] = index;

				indices.push_back(index);
				vertices.push_back(std::move(vertex));
			}
			else 
			{
				indices.push_back(uniqueVertices[vertex]);
			}
		}
	}

	return std::make_unique<Mesh>(Scope, vertices.data(), vertices.size(), indices.data(), indices.size());
}