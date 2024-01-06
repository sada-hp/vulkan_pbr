#include "scope.hpp"
#include "vulkan_objects/image.hpp"

namespace GRNoise
{
	TAuto<Image> GenerateWorley(const RenderScope& Scope, VkExtent2D imageSize, VkExtent2D cellSize);

	TAuto<Image> GenerateSimplex(const RenderScope& Scope, VkExtent2D imageSize, VkExtent3D cellSizeOcatves);
};