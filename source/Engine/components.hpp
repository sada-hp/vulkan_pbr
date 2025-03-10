#pragma once
#include "core.hpp"
#include "glm/glm.hpp"
#include "glm/common.hpp"
#include "Engine/enums.hpp"
#include "Engine/structs.hpp"
#include "glm/gtx/quaternion.hpp"
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

		struct EntityType
		{
			EntityType(Enums::EEntity Type)
				: m_type(Type)
			{

			}

			Enums::EEntity Get() const { return m_type; }

		protected:
			Enums::EEntity m_type;
		};
		/*
		* !@brief Projection matrix
		*/
		struct ProjectionMatrix
		{
			glm::mat4 matrix = glm::mat4(glm::vec4(0.f), glm::vec4(0.f), glm::vec4(0.f, 0.f, 0.f, -1.f), glm::vec4(0.f));

			GRAPI ProjectionMatrix& SetDepthRange(float Near, float Far)
			{
#if 0
				// From 0 to 1
				matrix[2][2] = -Far / (Far - Near);
				matrix[3][2] = -(Far * Near) / (Far - Near);
#else
				// Reversed depth
				matrix[2][2] = -Near / (Near - Far);
				matrix[3][2] = -(Far * Near) / (Near - Far);
#endif
				return *this;
			}

			GRAPI ProjectionMatrix& SetFOV(float Fov)
			{
				const float tanHalfFovy = glm::tan(Fov / 2.f);
				const float aspect = matrix[0][0] / (matrix[1][1] == 0.0 ? 1.0 : -matrix[1][1]);

				matrix[0][0] = aspect / tanHalfFovy;
				matrix[1][1] = -1.f / tanHalfFovy;

				return *this;
			}

			GRAPI ProjectionMatrix& SetAspect(float Aspect)
			{
				matrix[0][0] = (matrix[1][1] == 0.0 ? 1.0 : -matrix[1][1]) / Aspect;

				return *this;
			}
		};
		/*
		* !@brief World matrix
		*/
		template<typename TM, typename TV>
		struct TransformMatrix
		{
			glm::mat<3, 3, TM> orientation = glm::mat<3, 3, TM>(1.0);
			glm::vec<3, TV> offset = glm::vec<3, TV>(0.0);

			template<typename Type>
			GRAPI void SetFromMatrix(const glm::mat<4, 4, Type>& M)
			{
				orientation[0][0] = static_cast<TM>(M[0][0]);
				orientation[0][1] = static_cast<TM>(M[0][1]);
				orientation[0][2] = static_cast<TM>(M[0][2]);

				orientation[1][0] = static_cast<TM>(M[1][0]);
				orientation[1][1] = static_cast<TM>(M[1][1]);
				orientation[1][2] = static_cast<TM>(M[1][2]);

				orientation[2][0] = static_cast<TM>(M[2][0]);
				orientation[2][1] = static_cast<TM>(M[2][1]);
				orientation[2][2] = static_cast<TM>(M[2][2]);

				offset[0] = static_cast<TV>(M[3][0]);
				offset[1] = static_cast<TV>(M[3][1]);
				offset[2] = static_cast<TV>(M[3][2]);
			}

			template<typename Type>
			GRAPI glm::mat<4, 4, Type> GetMatrix()
			{
				return glm::mat<4, 4, Type>
				(
					static_cast<Type>(orientation[0][0]), static_cast<Type>(orientation[0][1]), static_cast<Type>(orientation[0][2]), static_cast<Type>(0.0),
					static_cast<Type>(orientation[1][0]), static_cast<Type>(orientation[1][1]), static_cast<Type>(orientation[1][2]), static_cast<Type>(0.0),
					static_cast<Type>(orientation[2][0]), static_cast<Type>(orientation[2][1]), static_cast<Type>(orientation[2][2]), static_cast<Type>(0.0),
					static_cast<Type>(offset[0]), static_cast<Type>(offset[1]), static_cast<Type>(offset[2]), static_cast<Type>(1.0)
				);
			}

			GRAPI TransformMatrix& SetOffset(const glm::vec<3, TV>& V)
			{
				offset = V;

				return *this;
			}

			GRAPI TransformMatrix& Translate(const glm::vec<3, TV>& V)
			{
				offset += GetRotation() * V;

				return *this;
			}

			GRAPI TransformMatrix& SetRotation(const glm::vec<3, TM>& R, const glm::vec<3, TM>& U, const glm::vec<3, TM>& F)
			{
				orientation[0] = R;
				orientation[1] = U;
				orientation[2] = F;

				return *this;
			}

			GRAPI TransformMatrix& SetRotation(const glm::vec<3, TM>& U, const glm::vec<3, TM>& F)
			{
				return SetRotation(glm::normalize(glm::cross(U, F)), U, F);
			}

			GRAPI TransformMatrix& SetRotation(const glm::mat<3, 3, TM>& M)
			{
				orientation = M;

				return *this;
			}

			GRAPI TransformMatrix& SetRotation(TM pitch, TM yaw, TM roll)
			{
				glm::qua<TM> q = glm::angleAxis(yaw, glm::vec<3, TM>(0, 1, 0));
				q = q * glm::angleAxis(roll, glm::vec<3, TM>(0, 0, 1));
				q = q * glm::angleAxis(-pitch, glm::vec<3, TM>(1, 0, 0));

				glm::mat<3, 3, TM> M = glm::mat3_cast(q);

				orientation[0] = glm::normalize(M[0]);
				orientation[1] = glm::normalize(M[1]);
				orientation[2] = glm::normalize(M[2]);

				return *this;
			}

			GRAPI TransformMatrix& Rotate(TM pitch, TM yaw, TM roll)
			{
				glm::qua<TM> q = glm::angleAxis(yaw, glm::vec<3, TM>(0, 1, 0));
				q = q * glm::angleAxis(roll, glm::vec<3, TM>(0, 0, 1));
				q = q * glm::angleAxis(-pitch, glm::vec<3, TM>(1, 0, 0));

				return SetRotation(glm::mat3_cast(q) * GetRotation());
			}

			GRAPI TransformMatrix& SetScale(TM x, TM y, TM z)
			{
				orientation[0] = glm::normalize(orientation[0]) * x;
				orientation[1] = glm::normalize(orientation[1]) * y;
				orientation[2] = glm::normalize(orientation[2]) * z;

				return *this;
			}

			GRAPI glm::vec<3, TV> GetOffset() const
			{
				return offset;
			}

			GRAPI glm::mat<3, 3, TM> GetRotation() const
			{
				return orientation;
			}

			GRAPI glm::vec<3, TM> GetForward() const
			{
				return glm::vec<3, TM>(orientation[2]);
			}

			GRAPI glm::vec<3, TM> GetRight() const
			{
				return glm::vec<3, TM>(orientation[0]);
			}

			GRAPI glm::vec<3, TM> GetUp() const
			{
				return glm::vec<3, TM>(orientation[1]);
			}
		};

		using WorldMatrix = TransformMatrix<float, double>;
	};
};