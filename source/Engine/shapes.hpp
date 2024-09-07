#pragma once
#include "core.hpp"
#include "Vulkan/mesh.hpp"
#include "Vulkan/scope.hpp"

namespace GR
{
	namespace Shapes
	{
		/*
		* !@brief This is a base class, use one of the specifications
		*/
		class Shape
		{
		protected:
			friend class VulkanBase;
			virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const = 0;

		public:
			virtual glm::vec3 GetDimensions() const = 0;
		};

		class Cube : public Shape
		{
		protected:
			friend class VulkanBase;
			GRAPI virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const override;

		public:
			glm::vec3 GetDimensions() const override
			{
				return glm::vec3(m_Scale * 0.5);
			}

		public:
			uint32_t m_EdgeSplits = 0u;
			float m_Scale = 1.f;
		};

		class Plane : public Shape
		{
		protected:
			friend class VulkanBase;
			GRAPI virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const override;
			
		public:
			glm::vec3 GetDimensions() const override
			{
				return glm::vec3(m_Scale * 0.5, 0.0, m_Scale * 0.5);
			}

		public:
			uint32_t m_EdgeSplits = 0u;
			float m_Scale = 1.f;
		};

		class Sphere : public Shape
		{
		protected:
			friend class VulkanBase;
			GRAPI virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const override;

		public:
			glm::vec3 GetDimensions() const override
			{
				return glm::vec3(m_Radius);
			}

		public:
			uint32_t m_Rings = 64u;
			uint32_t m_Slices = 64u;
			float m_Radius = 1.f;
		};

		class Mesh : public Shape
		{
		protected:
			friend class VulkanBase;
			GRAPI virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const override;

		public:
			glm::vec3 GetDimensions() const override
			{
				return glm::vec3(0.0);
			}

		public:
			std::string path;
		};

		class GeoClipmap : public Shape
		{
		protected:
			friend class VulkanBase;
			GRAPI virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const override;

		public:
			glm::vec3 GetDimensions() const override
			{
				return glm::vec3(0.0);
			}

		public:
			uint32_t m_Rings = 8;
			uint32_t m_VerPerRing = 255;
			float m_Scale = 1.f;
		};
	};
}