#include "scope.hpp"
#include "vulkan_objects/image.hpp"

namespace GRNoise
{
	TAuto<Image> GeneratePerlin(const RenderScope& Scope, VkExtent2D imageSize, uint32_t frequency, uint32_t octaves);

	TAuto<Image> GenerateWorley(const RenderScope& Scope, VkExtent2D imageSize, uint32_t frequency, uint32_t octaves);

	TAuto<Image> GenerateWorleyPerlin(const RenderScope& Scope, VkExtent2D imageSize, uint32_t frequency, uint32_t worley_octaves, uint32_t perlin_octaves);

	TAuto<Image> GenerateCloudNoise(const RenderScope& Scope, VkExtent3D imageSize, uint32_t frequency, uint32_t worley_octaves, uint32_t perlin_octaves);
};