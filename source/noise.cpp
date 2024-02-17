#include "pch.hpp"
#include "noise.hpp"
#include "vulkan_objects/pipeline.hpp"
#include "vulkan_objects/descriptor_set.hpp"

TAuto<Image> generate_noise(const char* shader, const RenderScope& Scope, VkFormat format, VkExtent3D imageSize, TVector<uint32_t> constants)
{
	TVector<uint32_t> queueFamilyIndices;
	queueFamilyIndices.push_back(Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex());
	queueFamilyIndices.push_back(Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex());
	VkImageCreateInfo noiseInfo{};
	noiseInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	noiseInfo.arrayLayers = 1u;
	noiseInfo.extent = imageSize;
	noiseInfo.format = format;
	noiseInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	noiseInfo.mipLevels = 1u;
	noiseInfo.flags = 0u;
	noiseInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	noiseInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	noiseInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
	noiseInfo.queueFamilyIndexCount = queueFamilyIndices.size();
	noiseInfo.pQueueFamilyIndices = queueFamilyIndices.data();
	noiseInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	noiseInfo.imageType = imageSize.depth == 1u ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
	VmaAllocationCreateInfo noiseAllocCreateInfo{};
	noiseAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	VkImageViewCreateInfo noiseImageView{};
	noiseImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	noiseImageView.viewType = imageSize.depth == 1u ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D;
	noiseImageView.format = format;
	noiseImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	noiseImageView.subresourceRange.baseArrayLayer = 0u;
	noiseImageView.subresourceRange.baseMipLevel = 0u;
	noiseImageView.subresourceRange.layerCount = 1u;
	noiseImageView.subresourceRange.levelCount = 1u;

	TAuto<Image> noise = std::make_unique<Image>(Scope);
	noise->CreateImage(noiseInfo, noiseAllocCreateInfo)
		.CreateImageView(noiseImageView)
		.CreateSampler(SamplerFlagBits::RepeatUVW)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	TAuto<ComputePipeline> noise_pipeline = std::make_unique<ComputePipeline>(Scope);
	TAuto<DescriptorSet> noise_set = std::make_unique<DescriptorSet>(Scope);
	noise_set->AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *noise)
		.Allocate();
	noise_pipeline->SetShaderName(shader) 
		.AddDescriptorLayout(noise_set->GetLayout());

	for (size_t i = 0; i < constants.size(); i++) {
		noise_pipeline->AddSpecializationConstant(i, constants[i]);
	}
	noise_pipeline->Construct();

	VkCommandBuffer cmd;
	Scope.GetQueue(VK_QUEUE_COMPUTE_BIT)
		.AllocateCommandBuffers(1, &cmd);
	::BeginOneTimeSubmitCmd(cmd);
	noise_set->BindSet(cmd, *noise_pipeline);
	noise_pipeline->BindPipeline(cmd);
	noise_pipeline->Dispatch(cmd, noise->GetExtent().width, noise->GetExtent().height, noise->GetExtent().depth);
	::EndCommandBuffer(cmd);
	Scope.GetQueue(VK_QUEUE_COMPUTE_BIT)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	noise->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	return noise;
}

TAuto<Image> GRNoise::GeneratePerlin(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t octaves)
{
	return generate_noise("perlin", Scope, VK_FORMAT_R8_UNORM, imageSize, { frequency, octaves });
}

TAuto<Image> GRNoise::GenerateWorley(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t octaves)
{
	return generate_noise("worley", Scope, VK_FORMAT_R8G8B8A8_UNORM, imageSize, { frequency, octaves });
}

TAuto<Image> GRNoise::GenerateWorleyPerlin(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t worley_octaves, uint32_t perlin_octaves)
{
	return generate_noise("worley_perlin", Scope, VK_FORMAT_R8_UNORM, imageSize, { frequency, worley_octaves, perlin_octaves });
}

TAuto<Image> GRNoise::GenerateCloudNoise(const RenderScope& Scope, VkExtent3D imageSize, uint32_t worley_frequency, uint32_t perlin_frequency, uint32_t worley_octaves, uint32_t perlin_octaves)
{
	int random = std::rand();
	int random2 = std::rand();
	uint32_t seed = *reinterpret_cast<char*>(&random) << 16 ^ *reinterpret_cast<char*>(&random2);
	return generate_noise("cloud_noise", Scope, VK_FORMAT_R8G8B8A8_UNORM, imageSize, { worley_frequency, perlin_frequency, worley_octaves, perlin_octaves, seed });
}