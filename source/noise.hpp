#include "scope.hpp"
#include "vulkan_objects/image.hpp"

namespace GRNoise
{
	TAuto<VulkanImage> GeneratePerlin(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t octaves);

	TAuto<VulkanImage> GenerateWorley(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t octaves);

	TAuto<VulkanImage> GenerateWorleyPerlin(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t worley_octaves, uint32_t perlin_octaves);

	TAuto<VulkanImage> GenerateCloudShapeNoise(const RenderScope& Scope, VkExtent3D imageSize, uint32_t worley_frequency, uint32_t perlin_frequency);

	TAuto<VulkanImage> GenerateCloudDetailNoise(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t octaves);

	TAuto<VulkanImage> GenerateCheckerBoard(const RenderScope& Scope, VkExtent2D imageSize, uint32_t frequency);

	TAuto<VulkanImage> GenerateSolidColor(const RenderScope& Scope, VkExtent2D imageSize, VkFormat Format, std::byte r = std::byte(0), std::byte g = std::byte(0), std::byte b = std::byte(0), std::byte a = std::byte(0));
};