#include "world.hpp"
#include "renderer.hpp"
#include "Engine/components.hpp"
#include "Vulkan/graphics_object.hpp"

namespace GR
{
	Entity World::AddShape(const Shapes::GeoClipmap& Descriptor)
	{
		Entity ent = Registry.create();
		Registry.emplace<Components::RGBColor>(ent);
		Registry.emplace<Components::MetallicOverride>(ent);
		Registry.emplace<Components::WorldMatrix>(ent);
		Registry.emplace<Components::DisplacementScale>(ent);
		Registry.emplace<Components::RoughnessMultiplier>(ent);

		return static_cast<const ::VulkanBase*>(&m_Scope)->_constructShape(ent, Registry, Descriptor);
	}

	Entity World::AddShape(const Shapes::Shape& Descriptor)
	{
		Entity ent = Registry.create();
		Registry.emplace<Components::RGBColor>(ent);
		Registry.emplace<Components::MetallicOverride>(ent);
		Registry.emplace<Components::WorldMatrix>(ent);
		Registry.emplace<Components::DisplacementScale>(ent);
		Registry.emplace<Components::RoughnessMultiplier>(ent);

		return static_cast<const ::VulkanBase*>(&m_Scope)->_constructShape(ent, Registry, Descriptor);
	}

	void World::BindTexture(Components::Resource<Texture>& Resource, const std::string& path)
	{
		VkFormat format;
		if (dynamic_cast<Components::NormalDisplacementMap*>(&Resource))
		{
			format = VK_FORMAT_R8G8B8A8_UNORM;
		}
		else if (dynamic_cast<Components::AORoughnessMetallicMap*>(&Resource))
		{
			format = VK_FORMAT_R8G8B8A8_UNORM;
		}
		else
		{
			format = VK_FORMAT_R8G8B8A8_SRGB;
		}

		Resource.Set(static_cast<const ::VulkanBase*>(&m_Scope)->_loadImage(path, format));
	}

	void World::DrawScene(double Delta)
	{
		auto renderer = static_cast<const ::VulkanBase*>(&m_Scope);
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
		static_cast<const ::VulkanBase*>(&m_Scope)->Wait();
		Registry.clear();
	}
};