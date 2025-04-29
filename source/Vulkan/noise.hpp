#include "scope.hpp"
#include "Vulkan/image.hpp"

namespace GRNoise
{
	std::unique_ptr<VulkanImage> GeneratePerlin(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t octaves);

	std::unique_ptr<VulkanImage> GenerateWorley(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t octaves);

	std::unique_ptr<VulkanImage> GenerateWorleyPerlin(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t worley_octaves, uint32_t perlin_octaves);

	std::unique_ptr<VulkanImage> GenerateCloudShapeNoise(const RenderScope& Scope, VkExtent3D imageSize, uint32_t worley_frequency, uint32_t perlin_frequency);

	std::unique_ptr<VulkanImage> GenerateCloudDetailNoise(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t octaves);
	
	std::unique_ptr<VulkanImage> GenerateWeatherCube(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t worley_octaves, uint32_t perlin_octaves);

	std::unique_ptr<VulkanImage> GenerateCheckerBoard(const RenderScope& Scope, VkExtent2D imageSize, uint32_t frequency);

	std::unique_ptr<VulkanImage> GenerateSolidColor(const RenderScope& Scope, VkExtent2D imageSize, VkFormat Format, std::byte r = std::byte(0), std::byte g = std::byte(0), std::byte b = std::byte(0), std::byte a = std::byte(0));
};