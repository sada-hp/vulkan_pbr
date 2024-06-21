#pragma once

#include "core.hpp"
#include "scope.hpp"
#include "vulkan_objects/mesh.hpp"

namespace GRShape
{
	class Shape
	{
	protected:
		friend class VulkanBase;
		virtual TAuto<Mesh> Generate(const RenderScope& Scope) const = 0;
	};

	GRAPI class Cube : public Shape
	{
	protected:
		friend class VulkanBase;
		GRAPI virtual TAuto<Mesh> Generate(const RenderScope& Scope) const override;

	public:
		uint32_t edge_splits = 0u;
		float scale = 1.f;
	};

	GRAPI class Plane : public Shape
	{
	protected:
		friend class VulkanBase;
		GRAPI virtual TAuto<Mesh> Generate(const RenderScope& Scope) const override;

	public:
		uint32_t edge_splits = 0u;
		float scale = 1.f;
	};

	GRAPI class Sphere : public Shape
	{
	protected:
		friend class VulkanBase;
		GRAPI virtual TAuto<Mesh> Generate(const RenderScope& Scope) const override;
	
	public:
		uint32_t rings = 64u;
		uint32_t slices = 64u;
		float radius = 1.f;
	};
}