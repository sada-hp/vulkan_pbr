#pragma once
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/hash.hpp>

struct Vertex
{
	static const VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static const TArray<VkVertexInputAttributeDescription, 1> getAttributeDescriptions()
	{
		TArray<VkVertexInputAttributeDescription, 1> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		return attributeDescriptions;
	}

	bool operator==(const Vertex& other) const
	{
		return position == other.position && meshIndex == other.meshIndex;
	}

	uint32_t meshIndex = 0;
	glm::vec3 position;
};

template<>
struct std::hash<Vertex>
{
	size_t operator()(Vertex const& vertex) const
	{
		return ((std::hash<glm::vec3>()(vertex.position) ^ (std::hash<uint32_t>()(vertex.meshIndex))) >> 1);
	}
};