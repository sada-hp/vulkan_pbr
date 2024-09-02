#pragma once
#include "pch.hpp"
#include "core.hpp"
#include "shapes.hpp"
#include "entt/entt.hpp"
#include "components.hpp"

namespace GR
{
	class Renderer;

	using Entity = entt::entity;

	class World
	{
	private:
		const Renderer& m_Scope;

	public:
		entt::registry Registry;
		
		World(const Renderer& Context) 
			: m_Scope(Context) {};

		~World() = default;

		GRAPI GR::Entity AddShape(const GR::Shape& Descriptor);

		GRAPI void BindTexture(GRComponents::Resource<Texture>& Resource, const std::string& path);

		GRAPI void DrawScene();

		GRAPI void Clear();

		template<typename Type, typename... Args>
		GRAPI decltype(auto) EmplaceComponent(Entity ent, Args&& ...args)
		{
			return Registry.emplace_or_replace<Type>(ent, args...);
		}

		template<typename... Type>
		GRAPI decltype(auto) GetComponent(const Entity ent)
		{
			return Registry.get<Type...>(ent);
		}
	};
};