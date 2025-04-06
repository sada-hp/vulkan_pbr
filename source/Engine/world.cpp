#include "world.hpp"
#include "renderer.hpp"
#include "Engine/components.hpp"
#include "Vulkan/graphics_object.hpp"

namespace GR
{
	Entity World::AddShape(const Shapes::GeoClipmap& Descriptor)
	{
		if (m_TerrainEntity != entt::entity(-1))
		{
			Registry.destroy(m_TerrainEntity);
		}

		m_TerrainEntity = Registry.create();
		Registry.emplace<Components::RGBColor>(m_TerrainEntity);
		Registry.emplace<Components::MetallicOverride>(m_TerrainEntity);
		Registry.emplace<Components::DisplacementScale>(m_TerrainEntity);
		Registry.emplace<Components::RoughnessMultiplier>(m_TerrainEntity);
		Registry.emplace<Components::EntityType>(m_TerrainEntity, Enums::EEntity::Terrain);

		return static_cast<VulkanBase*>(m_Scope)->_constructShape(m_TerrainEntity, Registry, Descriptor);
	}

	Entity World::AddShape(const Shapes::Shape& Descriptor)
	{
		Entity ent = Registry.create();
		Registry.emplace<Components::RGBColor>(ent);
		Registry.emplace<Components::MetallicOverride>(ent);
		Registry.emplace<Components::WorldMatrix>(ent);
		Registry.emplace<Components::DisplacementScale>(ent);
		Registry.emplace<Components::RoughnessMultiplier>(ent);
		Registry.emplace<Components::EntityType>(ent, Enums::EEntity::Shape);

		return static_cast<VulkanBase*>(m_Scope)->_constructShape(ent, Registry, Descriptor);
	}

	void World::BindTexture(Components::Resource<Texture>& Resource, const std::string& path)
	{
		Resource.Set(static_cast<VulkanBase*>(m_Scope)->_loadImage({ path }, VK_FORMAT_R8G8B8A8_UNORM));
	}

	void World::BindTexture(Components::Resource<Texture>& Resource, const std::vector<std::string>& paths)
	{
		Resource.Set(static_cast<VulkanBase*>(m_Scope)->_loadImage(paths, VK_FORMAT_R8G8B8A8_UNORM));
	}

	void World::DrawScene(double Delta)
	{
		auto renderer = static_cast<VulkanBase*>(m_Scope);

		if (m_TerrainEntity != entt::entity(-1))
		{
			PBRConstants constants{};
			constants.Color = glm::vec4(Registry.get<Components::RGBColor>(m_TerrainEntity).Value, 1.0);
			constants.RoughnessMultiplier = Registry.get<Components::RoughnessMultiplier>(m_TerrainEntity).Value;
			constants.Metallic = Registry.get<Components::MetallicOverride>(m_TerrainEntity).Value;
			constants.HeightScale = Registry.get<Components::DisplacementScale>(m_TerrainEntity).Value;

			PBRObject& tro = Registry.get<PBRObject>(m_TerrainEntity);
			if (tro.is_dirty())
			{
				renderer->_updateTerrain(m_TerrainEntity, Registry);
			}

			renderer->_drawTerrain(tro, constants);
		}

		auto view = Registry.view<PBRObject, Components::WorldMatrix>();
		for (const auto& [ent, gro, world] : view.each())
		{
			PBRConstants constants{};
			constants.Offset = world.GetOffset();
			constants.Orientation = glm::mat3x4(world.GetRotation());
			constants.Color = glm::vec4(Registry.get<Components::RGBColor>(ent).Value, 1.0);
			constants.RoughnessMultiplier = Registry.get<Components::RoughnessMultiplier>(ent).Value;
			constants.Metallic = Registry.get<Components::MetallicOverride>(ent).Value;
			constants.HeightScale = Registry.get<Components::DisplacementScale>(ent).Value;

			if (gro.is_dirty())
			{
				renderer->_updateObject(ent, Registry);
			}

			renderer->_drawObject(gro, constants);
		}
	}

	void World::Clear()
	{
		static_cast<VulkanBase*>(m_Scope)->Wait();
		Registry.clear();
	}
};