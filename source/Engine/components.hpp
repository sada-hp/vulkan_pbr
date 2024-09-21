#pragma once
#include "core.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/glm.hpp"
#include "glm/common.hpp"
#include "Engine/structs.hpp"
/*
* List of engine-specific components
*/
namespace GR
{
	namespace Components
	{
		/*
		* !@brief Resource holder, resource logic uses shared pointer
		*/
		template<typename Type, typename... Args>
		struct Resource
		{
		public:
			/*
			* @param[in] resource - heap pointer to resource data
			* @param[in] flagptr - if given, specifies a bool value,
			* which is gonna be updated to true each time 'Set' function is called for this resource
			*/
			Resource(std::shared_ptr<Type> resource, bool* flagptr = nullptr)
				: m_Resource(resource), m_Dirty(flagptr)
			{

			}
			/*
			* !@brief Sets the new value of this resource
			*
			* @param[in] r - new pointer
			*/
			GRAPI virtual void Set(std::shared_ptr<Type> r)
			{
				m_Resource = r;

				if (m_Dirty)
					*m_Dirty = true;
			}
			/*
			* !@brief Gets the resource pointer
			*
			* @return Shared pointer to the resource
			*/
			GRAPI std::shared_ptr<Type> Get() const
			{
				return m_Resource;
			}

		private:
			std::shared_ptr<Type> m_Resource = nullptr;
			bool* m_Dirty = nullptr;
		};
		/*
		* !@brief Albedo map defines color map in PBR pipeline
		*/
		struct AlbedoMap : Resource<Texture>
		{
		public:
			AlbedoMap(std::shared_ptr<Texture> resource, bool* flagptr = nullptr)
				: Resource(resource, flagptr) { }
		private:
		};
		/*
		* !@brief Combined Normal(rgb) and Height(a) map used in PBR pipeline
		*/
		struct NormalDisplacementMap : Resource<Texture>
		{
		public:
			NormalDisplacementMap(std::shared_ptr<Texture> resource, bool* flagptr = nullptr)
				: Resource(resource, flagptr) { }
		private:
		};
		/*
		* !@brief Combined AO(r), Roughness(g) and Metallic(b) map used in PBR pipeline
		*/
		struct AORoughnessMetallicMap : Resource<Texture>
		{
		public:
			AORoughnessMetallicMap(std::shared_ptr<Texture> resource, bool* flagptr = nullptr)
				: Resource(resource, flagptr) { }
		private:
		};
		/*
		* !@brief Multiplies roughness by this value
		*/
		struct RoughnessMultiplier
		{
		public:
			float Value = 1.0;
		private:
		};
		/*
		* !@brief If set to non zero, overrides metallic value of the material
		*/
		struct MetallicOverride
		{
		public:
			float Value = 0.0;
		private:
		};
		/*
		* !@brief Scale of Parallax mapping
		*/
		struct DisplacementScale
		{
		public:
			float Value = 1.0;
		private:
		};
		/*
		* !@brief Multiplies original color by this value
		*/
		struct RGBColor
		{
		public:
			glm::vec3 Value = glm::vec3(1.0);
		private:
		};
		/*
		* !@brief Projection matrix
		*/
		struct ProjectionMatrix
		{
			glm::mat4 matrix = glm::mat4(glm::vec4(0.f), glm::vec4(0.f), glm::vec4(0.f, 0.f, 0.f, -1.f), glm::vec4(0.f));

			GRAPI ProjectionMatrix& SetDepthRange(float Near, float Far)
			{
				// Reverse depth
#if 0
				matrix[2][2] = -Far / (Far - Near);
				matrix[3][2] = -(Far * Near) / (Far - Near);
#else
				matrix[2][2] = -Near / (Near - Far);
				matrix[3][2] = -(Far * Near) / (Near - Far);
#endif
				return *this;
			}

			GRAPI ProjectionMatrix& SetFOV(float Fov, float Aspect)
			{
				const float tanHalfFovy = glm::tan(Fov / 2.f);

				matrix[0][0] = 1.f / (Aspect * tanHalfFovy);
				matrix[1][1] = -1.f / (tanHalfFovy);

				return *this;
			}
		};
		/*
		* !@brief World matrix
		*/
		template<typename T>
		struct TransformMatrix
		{
			glm::mat<4, 4, T> matrix = glm::mat<4, 4, T>(1.0);

			GRAPI TransformMatrix& SetOffset(const glm::vec<3, T>& V)
			{
				matrix[3] = glm::vec<4, T>(V, 1.0);

				return *this;
			}

			GRAPI TransformMatrix& Translate(const glm::vec<3, T>& V)
			{
				matrix[3] += glm::vec<4, T>(GetRotation() * V, 0.0);

				return *this;
			}

			GRAPI TransformMatrix& SetRotation(const glm::vec<3, T>& R, const glm::vec<3, T>& U, const glm::vec<3, T>& F)
			{
				matrix[0] = glm::vec<4, T>(R, 0.0);
				matrix[1] = glm::vec<4, T>(U, 0.0);
				matrix[2] = glm::vec<4, T>(F, 0.0);

				return *this;
			}

			GRAPI TransformMatrix& SetRotation(const glm::vec<3, T>& U, const glm::vec<3, T>& F)
			{
				return SetRotation(glm::normalize(glm::cross(U, F)), U, F);
			}

			GRAPI TransformMatrix& SetRotation(const glm::mat<3, 3, T>& M)
			{
				matrix[0] = glm::vec<4, T>(M[0], 0.0);
				matrix[1] = glm::vec<4, T>(M[1], 0.0);
				matrix[2] = glm::vec<4, T>(M[2], 0.0);

				return *this;
			}

			GRAPI TransformMatrix& SetRotation(T pitch, T yaw, T roll)
			{
				glm::qua<T> q = glm::angleAxis(yaw, glm::vec<3, T>(0, 1, 0));
				q = q * glm::angleAxis(roll, glm::vec<3, T>(0, 0, 1));
				q = q * glm::angleAxis(-pitch, glm::vec<3, T>(1, 0, 0));

				glm::mat<3, 3, T> M = glm::mat3_cast(q);

				matrix[0] = glm::vec<4, T>(glm::normalize(M[0]), 0.0);
				matrix[1] = glm::vec<4, T>(glm::normalize(M[1]), 0.0);
				matrix[2] = glm::vec<4, T>(glm::normalize(M[2]), 0.0);

				return *this;
			}

			GRAPI TransformMatrix& Rotate(T pitch, T yaw, T roll)
			{
				glm::qua<T> q = glm::angleAxis(yaw, glm::vec<3, T>(0, 1, 0));
				q = q * glm::angleAxis(roll, glm::vec<3, T>(0, 0, 1));
				q = q * glm::angleAxis(-pitch, glm::vec<3, T>(1, 0, 0));

				return SetRotation(glm::mat3_cast(q) * GetRotation());
			}

			GRAPI TransformMatrix& SetScale(T x, T y, T z)
			{
				glm::mat<3, 3, T> mat = glm::mat<3, 3, T>(matrix);
				mat[0] = glm::normalize(mat[0]) * x;
				mat[1] = glm::normalize(mat[1]) * y;
				mat[2] = glm::normalize(mat[2]) * z;

				matrix[0] = glm::vec<4, T>(mat[0], 0.0);
				matrix[1] = glm::vec<4, T>(mat[1], 0.0);
				matrix[2] = glm::vec<4, T>(mat[2], 0.0);

				return *this;
			}

			GRAPI glm::vec<3, T> GetOffset() const
			{
				return glm::vec<3, T>(matrix[3]);
			}

			GRAPI glm::mat<3, 3, T> GetRotation() const
			{
				return glm::mat<3, 3, T>(matrix);
			}

			GRAPI glm::vec<3, T> GetForward() const
			{
				return glm::vec<3, T>(matrix[2]);
			}

			GRAPI glm::vec<3, T> GetRight() const
			{
				return glm::vec<3, T>(matrix[0]);
			}

			GRAPI glm::vec<3, T> GetUp() const
			{
				return glm::vec<3, T>(matrix[1]);
			}
		};

		using WorldMatrix = TransformMatrix<float>;
		using ViewMatrix = TransformMatrix<double>;
	};
};