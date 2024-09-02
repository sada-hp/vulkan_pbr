#pragma once
#include "core.hpp"
#include "Vulkan/mesh.hpp"
#include "Vulkan/scope.hpp"

namespace GR
{
	/*
	* !@brief This is a base class, use one of the specifications
	*/
	class Shape
	{
	protected:
		friend class VulkanBase;
		virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const = 0;
	};

	class Cube final : public Shape
	{
	protected:
		friend class VulkanBase;
		GRAPI virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const override;

	public:
		uint32_t m_EdgeSplits = 0u;
		float m_Scale = 1.f;
	};

	class Plane final : public Shape
	{
	protected:
		friend class VulkanBase;
		GRAPI virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const override;

	public:
		uint32_t m_EdgeSplits = 0u;
		float m_Scale = 1.f;
	};

	class Sphere final : public Shape
	{
	protected:
		friend class VulkanBase;
		GRAPI virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const override;
	
	public:
		uint32_t m_Rings = 64u;
		uint32_t m_Slices = 64u;
		float m_Radius = 1.f;
	};

	class Mesh final : public Shape
	{
	protected:
		friend class VulkanBase;
		GRAPI virtual std::unique_ptr<VulkanMesh> Generate(const RenderScope& Scope) const override;

	public:
		std::string path;
	};
}