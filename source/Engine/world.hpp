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
	protected:
		const Renderer& m_Scope;

	public:
		entt::registry Registry;
		
		World(const Renderer& Context) 
			: m_Scope(Context) {};

		virtual ~World() { Clear(); };

		GRAPI virtual Entity AddShape(const Shapes::Shape& Descriptor);

		GRAPI virtual void DrawScene(double Delta);

		GRAPI virtual void Clear();

		GRAPI void BindTexture(Components::Resource<Texture>& Resource, const std::string& path);

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