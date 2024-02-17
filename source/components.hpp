#pragma once
#include "core.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/glm.hpp"
#include "glm/common.hpp"
#include "math.hpp"
/*
* List of engine-specific components
*/
namespace GRComponents
{
	struct Color
	{
		glm::vec3 RGB = glm::vec3(1.0);
	};

	struct Transform
	{
		glm::mat4 matrix = glm::mat4(1.0);

		GRAPI void SetOffset(const TVec3& V)
		{
			matrix = glm::mat4(glm::vec4(1.0, 0.0, 0.0, 0.0),
							glm::vec4(0.0, 1.0, 0.0, 0.0),
							glm::vec4(0.0, 0.0, 1.0, 0.0),
							glm::vec4(V, 1.0)) * glm::mat4(glm::mat3(matrix));
		}

		GRAPI void Translate(const TVec3& V)
		{
			matrix[3] += glm::vec4(V, 1.0);
		}

		GRAPI void SetRotation(const TVec3& R, const TVec3& U, const TVec3& F)
		{
			glm::vec4 offset = glm::vec4(GetOffset(), 1.0);

			matrix = glm::mat4(1.f);
			matrix[0] = glm::vec4(R, 0.0);
			matrix[1] = glm::vec4(U, 0.0);
			matrix[2] = glm::vec4(F, 0.0);

			matrix = glm::mat4(glm::vec4(1.0, 0.0, 0.0, 0.0),
							glm::vec4(0.0, 1.0, 0.0, 0.0),
							glm::vec4(0.0, 0.0, 1.0, 0.0),
							offset) * matrix;
		}

		GRAPI void SetRotation(const TVec3& U, const TVec3& F)
		{
			SetRotation(glm::normalize(glm::cross(U, F)), U, F);
		}

		GRAPI void SetRotation(const TMat3& M)
		{
			glm::vec4 offset = glm::vec4(GetOffset(), 1.0);

			matrix = glm::mat4(1.f);
			matrix[0] = glm::vec4(M[0], 0.0);
			matrix[1] = glm::vec4(M[1], 0.0);
			matrix[2] = glm::vec4(M[2], 0.0);

			matrix = glm::mat4(glm::vec4(1.0, 0.0, 0.0, 0.0),
							glm::vec4(0.0, 1.0, 0.0, 0.0),
							glm::vec4(0.0, 0.0, 1.0, 0.0),
							offset) * matrix;
		}

		GRAPI void SetRotation(float pitch, float yaw, float roll)
		{
			TQuat q = glm::angleAxis(roll, glm::vec3(0, 0, 1));
			q = q * glm::angleAxis(-pitch, glm::vec3(1, 0, 0));
			q = q * glm::angleAxis(yaw, glm::vec3(0, 1, 0));

			SetRotation(glm::mat3(glm::mat4_cast(q)));
		}

		GRAPI void Rotate(float pitch, float yaw, float roll)
		{
			TQuat q = glm::angleAxis(roll, glm::vec3(0, 0, 1));
			q = q * glm::angleAxis(-pitch, glm::vec3(1, 0, 0));
			q = q * glm::angleAxis(yaw, GetUp());

			matrix = glm::mat4_cast(q) * matrix;
			matrix[3] = glm::vec4(glm::floor(glm::vec3(matrix[3]) * 100.f) / 100.f, 1.0);
		}

		GRAPI TVec3 GetOffset() const
		{
			return glm::vec3(matrix[3]);
		}

		GRAPI TMat3 GetRotation() const
		{
			return glm::mat3(matrix);
		}

		GRAPI TVec3 GetForward() const
		{
			return glm::normalize(glm::vec3(matrix[2]));
		}

		GRAPI TVec3 GetRight() const
		{
			return glm::normalize(glm::vec3(matrix[0]));
		}

		GRAPI TVec3 GetUp() const
		{
			return glm::normalize(glm::vec3(matrix[1]));
		}
	};
};