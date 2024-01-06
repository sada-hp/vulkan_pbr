#include "pch.hpp"
#include "noise.hpp"
#include "vulkan_objects/pipeline.hpp"
#include "vulkan_objects/descriptor_set.hpp"

TAuto<Image> generate_noise(const char* shader, const RenderScope& Scope, VkExtent2D imageSize, TVector<uint32_t> constants)
{
	TAuto<Image> noise;
	TAuto<ComputePipeline> noise_pipeline;
	TAuto<DescriptorSet> noise_set;

	TVector<uint32_t> queueFamilyIndices;
	queueFamilyIndices.push_back(Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
	queueFamilyIndices.push_back(Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
	VkImageCreateInfo noiseInfo{};
	noiseInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	noiseInfo.arrayLayers = 1u;
	noiseInfo.extent = { imageSize.width, imageSize.height, 1u };
	noiseInfo.format = VK_FORMAT_R8_UNORM;
	noiseInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	noiseInfo.mipLevels = 1u;
	noiseInfo.flags = 0u;
	noiseInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	noiseInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	noiseInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
	noiseInfo.queueFamilyIndexCount = queueFamilyIndices.size();
	noiseInfo.pQueueFamilyIndices = queueFamilyIndices.data();
	noiseInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	noiseInfo.imageType = VK_IMAGE_TYPE_2D;
	VmaAllocationCreateInfo noiseAllocCreateInfo{};
	noiseAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	VkImageViewCreateInfo noiseImageView{};
	noiseImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	noiseImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	noiseImageView.format = VK_FORMAT_R8_UNORM;
	noiseImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	noiseImageView.subresourceRange.baseArrayLayer = 0u;
	noiseImageView.subresourceRange.baseMipLevel = 0u;
	noiseImageView.subresourceRange.layerCount = 1u;
	noiseImageView.subresourceRange.levelCount = 1u;

	noise = std::make_unique<Image>(Scope);
	noise->CreateImage(noiseInfo, noiseAllocCreateInfo)
		.CreateImageView(noiseImageView)
		.CreateSampler(SamplerFlagBits::AnisatropyEnabled)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	noise_pipeline = std::make_unique<ComputePipeline>(Scope);
	noise_set = std::make_unique<DescriptorSet>(Scope);
	noise_set->AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *noise)
		.Allocate();
	noise_pipeline->SetShaderName(shader) 
		.AddDescriptorLayout(noise_set->GetLayout());

	for (size_t i = 0; i < constants.size(); i++) {
		noise_pipeline->AddSpecializationConstant(3u + i, constants[i]);
	}
	noise_pipeline->Construct();

	VkCommandBuffer cmd;
	Scope.GetQueue(VK_QUEUE_COMPUTE_BIT)
		.AllocateCommandBuffers(1, &cmd);
	::BeginOneTimeSubmitCmd(cmd);
	noise_set->BindSet(cmd, *noise_pipeline);
	noise_pipeline->BindPipeline(cmd);
	noise_pipeline->Dispatch(cmd, noise->GetExtent().width, noise->GetExtent().height, 1u);
	::EndCommandBuffer(cmd);
	Scope.GetQueue(VK_QUEUE_COMPUTE_BIT)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	noise->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	return noise;
}

TAuto<Image> GRNoise::GenerateWorley(const RenderScope& Scope, VkExtent2D imageSize, VkExtent2D cellSize)
{
	return generate_noise("worley", Scope, imageSize, { cellSize.height, cellSize.width });
}

TAuto<Image> GRNoise::GenerateSimplex(const RenderScope& Scope, VkExtent2D imageSize, VkExtent3D cellSizeOcatves)
{
	return generate_noise("simplex", Scope, imageSize, { cellSizeOcatves.height, cellSizeOcatves.width, cellSizeOcatves.depth });
}