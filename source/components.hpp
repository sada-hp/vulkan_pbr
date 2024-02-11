#pragma once
#include "vulkan_objects/pipeline.hpp"
#include "vulkan_objects/image.hpp"
#include "vulkan_objects/mesh.hpp"
#include "vulkan_objects/descriptor_set.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/glm.hpp"

struct Name
{
	Name(const char* in)
		: name(in)
	{
	}

	const std::string name;
};

struct VulkanGraphicsObject
{
	VulkanGraphicsObject(RenderScope& Scope)
		: descriptor_set(Scope), pipeline(Scope)
	{
	}

	TAuto<Mesh> mesh;
	TAuto<Image> texture;

	DescriptorSet descriptor_set;
	GraphicsPipeline pipeline;
};

struct WorldMatrix
{
	void SetRotation(float Pitch, float Yaw, float Roll)
	{

	}

	void SetUp(const glm::vec3& Up)
	{
		rotation[0].y = Up.x;
		rotation[1].y = Up.y;
		rotation[2].y = Up.z;
		isDirty = true;
	}

	void SetRight(const glm::vec3& Right)
	{
		rotation[0].x = Right.x;
		rotation[1].x = Right.y;
		rotation[2].x = Right.z;
		isDirty = true;
	}

	void SetForward(const glm::vec3& Forward)
	{
		rotation[0].z = Forward.x;
		rotation[1].z = Forward.y;
		rotation[2].z = Forward.z;
		isDirty = true;
	}

	void SetOffset(const glm::vec3& Offset)
	{
		offset = Offset;
		isDirty = true;
	}

	const glm::mat4& GetMatrix() const
	{
		if (isDirty)
		{
			matrix = glm::translate(glm::mat4(rotation), offset);
			isDirty = false;
		}

		return matrix;
	}

private:
	glm::mat3 rotation = glm::mat3(1.0);
	glm::vec3 offset = glm::vec3(0.0);

	mutable glm::mat4 matrix;
	mutable bool isDirty = true;
};