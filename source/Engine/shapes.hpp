#pragma once
#include "core.hpp"
#include "scope.hpp"
#include "Engine/math.hpp"
#include "Vulkan/mesh.hpp"

namespace GRShape
{
	/*
	* !@brief This is a base class, use one of the specifications
	*/
	class Shape
	{
	protected:
		friend class VulkanBase;
		virtual TAuto<Mesh> Generate(const RenderScope& Scope) const = 0;
	};

	class Cube : public Shape
	{
	protected:
		friend class VulkanBase;
		GRAPI virtual TAuto<Mesh> Generate(const RenderScope& Scope) const override;

	public:
		uint32_t m_EdgeSplits = 0u;
		float m_Scale = 1.f;
	};

	class Plane : public Shape
	{
	protected:
		friend class VulkanBase;
		GRAPI virtual TAuto<Mesh> Generate(const RenderScope& Scope) const override;

	public:
		uint32_t m_EdgeSplits = 0u;
		float m_Scale = 1.f;
	};

	class Sphere : public Shape
	{
	protected:
		friend class VulkanBase;
		GRAPI virtual TAuto<Mesh> Generate(const RenderScope& Scope) const override;
	
	public:
		uint32_t m_Rings = 64u;
		uint32_t m_Slices = 64u;
		float m_Radius = 1.f;
	};
}