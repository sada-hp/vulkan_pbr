#pragma once
#include "core.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/glm.hpp"
#include "glm/common.hpp"
#include "math.hpp"
#include "file_manager.hpp"
/*
* List of engine-specific components
*/
namespace GRComponents
{
	template<typename Type, typename... Args>
	struct Resource
	{
		Resource(TShared<Type> resource, bool* flagptr = nullptr)
			: res(resource), dirty_flag(flagptr)
		{

		}

		GRAPI void Set(TShared<Type> r)
		{
			res = r;

			if (dirty_flag)
				*dirty_flag = true;
		}

		GRAPI TShared<Type> Get() const
		{
			return res;
		}

	private:
		TShared<Type> res = nullptr;
		bool* dirty_flag = nullptr;
	};

	struct AlbedoMap : Resource<Image>
	{
		AlbedoMap(TShared<Image> resource, bool* flagptr = nullptr)
			: Resource(resource, flagptr) { }
	};

	struct NormalMap : Resource<Image>
	{
		NormalMap(TShared<Image> resource, bool* flagptr = nullptr)
			: Resource(resource, flagptr) { }
	};

	struct RoughnessMap : Resource<Image>
	{
		RoughnessMap(TShared<Image> resource, bool* flagptr = nullptr)
			: Resource(resource, flagptr) { }
	};

	struct MetallicMap : Resource<Image>
	{
		MetallicMap(TShared<Image> resource, bool* flagptr = nullptr)
			: Resource(resource, flagptr) { }
	};

	struct AmbientMap : Resource<Image>
	{
		AmbientMap(TShared<Image> resource, bool* flagptr = nullptr)
			: Resource(resource, flagptr) { }
	};

	struct DisplacementMap : Resource<Image>
	{
		DisplacementMap(TShared<Image> resource, bool* flagptr = nullptr)
			: Resource(resource, flagptr) { }
	};

	struct RoughnessMultiplier
	{
		float R = 1.0;
	};

	struct MetallicOverride
	{
		float M = 0.0;
	};

	struct DisplacementScale
	{
		float H = 1.0;
	};

	struct Color
	{
		glm::vec3 RGB = glm::vec3(1.0);
	};

	struct Projection
	{
		glm::mat4 matrix = glm::mat4(glm::vec4(0.f), glm::vec4(0.f), glm::vec4(0.f, 0.f, 0.f, -1.f), glm::vec4(0.f));

		GRAPI Projection& SetDepthRange(float Near, float Far)
		{
			matrix[2][2] = -(Far + Near) / (Far - Near);
			matrix[3][2] = -2.f * (Far * Near) / (Far - Near);

			return *this;
		}

		GRAPI Projection& SetFOV(float Fov, float Aspect)
		{
			const float tanHalfFovy = glm::tan(Fov / 2.f);

			matrix[0][0] = 1.f / (Aspect * tanHalfFovy);
			matrix[1][1] = -1.f / (tanHalfFovy);

			return *this;
		}
	};

	struct Transform
	{
		glm::mat4 matrix = glm::mat4(1.0);

		GRAPI Transform& SetOffset(const TVec3& V)
		{
			matrix[3] = glm::vec4(V, 1.0);

			return *this;
		}

		GRAPI Transform& Translate(const TVec3& V)
		{
			matrix[3] += glm::vec4(GetRotation() * V, 1.0);

			return *this;
		}

		GRAPI Transform& SetRotation(const TVec3& R, const TVec3& U, const TVec3& F)
		{
			matrix[0] = glm::vec4(R, 0.0);
			matrix[1] = glm::vec4(U, 0.0);
			matrix[2] = glm::vec4(F, 0.0);

			return *this;
		}

		GRAPI Transform& SetRotation(const TVec3& U, const TVec3& F)
		{
			return SetRotation(glm::normalize(glm::cross(U, F)), U, F);
		}

		GRAPI Transform& SetRotation(const TMat3& M)
		{
			matrix[0] = glm::vec4(M[0], 0.0);
			matrix[1] = glm::vec4(M[1], 0.0);
			matrix[2] = glm::vec4(M[2], 0.0);

			return *this;
		}

		GRAPI Transform& SetRotation(float pitch, float yaw, float roll)
		{
			TQuat q = glm::angleAxis(yaw, glm::vec3(0, 1, 0));
			q = q * glm::angleAxis(roll, glm::vec3(0, 0, 1));
			q = q * glm::angleAxis(-pitch, glm::vec3(1, 0, 0));

			glm::mat3 M = glm::mat3_cast(q);

			matrix[0] = glm::vec4(glm::normalize(M[0]), 0.0);
			matrix[1] = glm::vec4(glm::normalize(M[1]), 0.0);
			matrix[2] = glm::vec4(glm::normalize(M[2]), 0.0);

			return *this;
		}

		GRAPI Transform& Rotate(float pitch, float yaw, float roll)
		{
			TQuat q = glm::angleAxis(yaw, glm::vec3(0, 1, 0));
			q = q * glm::angleAxis(roll, glm::vec3(0, 0, 1));
			q = q * glm::angleAxis(-pitch, glm::vec3(1, 0, 0));

			return SetRotation(glm::mat3_cast(q) * GetRotation());
		}

		GRAPI Transform& SetScale(float x, float y, float z)
		{
			glm::mat3 mat = glm::mat3(matrix);
			mat[0] = glm::normalize(mat[0]) * x;
			mat[1] = glm::normalize(mat[1]) * y;
			mat[2] = glm::normalize(mat[2]) * z;

			matrix[0] = glm::vec4(mat[0], 0.0);
			matrix[1] = glm::vec4(mat[1], 0.0);
			matrix[2] = glm::vec4(mat[2], 0.0);

			return *this;
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
			return glm::vec3(matrix[2]);
		}

		GRAPI TVec3 GetRight() const
		{
			return glm::vec3(matrix[0]);
		}

		GRAPI TVec3 GetUp() const
		{
			return glm::vec3(matrix[1]);
		}
	};
};