#include "world.hpp"
#include "renderer.hpp"
#include "Engine/components.hpp"
#include "Vulkan/graphics_object.hpp"

namespace GR
{
	GR::Entity World::AddShape(const GR::Shape& Descriptor)
	{
		Entity ent = Registry.create();
		Registry.emplace<GRComponents::RGBColor>(ent);
		Registry.emplace<GRComponents::MetallicOverride>(ent);
		Registry.emplace<GRComponents::Transform<float>>(ent);
		Registry.emplace<GRComponents::DisplacementScale>(ent);
		Registry.emplace<GRComponents::RoughnessMultiplier>(ent);

		return static_cast<const ::VulkanBase*>(&m_Scope)->_constructShape(ent, Registry, Descriptor);
	}

	void World::BindTexture(GRComponents::Resource<Texture>& Resource, const std::string& path)
	{
		VkFormat format;
		if (dynamic_cast<GRComponents::NormalDisplacementMap*>(&Resource))
		{
			format = VK_FORMAT_R8G8B8A8_UNORM;
		}
		else if (dynamic_cast<GRComponents::AORoughnessMetallicMap*>(&Resource))
		{
			format = VK_FORMAT_R8G8B8A8_UNORM;
		}
		else
		{
			format = VK_FORMAT_R8G8B8A8_SRGB;
		}

		Resource.Set(static_cast<const ::VulkanBase*>(&m_Scope)->_loadImage(path, format));
	}

	void World::DrawScene()
	{
		auto renderer = static_cast<const ::VulkanBase*>(&m_Scope);
		auto view = Registry.view<PBRObject, GRComponents::Transform<float>>();
		for (const auto& [ent, gro, world] : view.each())
		{
			PBRConstants constants{};
			constants.World = world.matrix;
			constants.Color = glm::vec4(Registry.get<GRComponents::RGBColor>(ent).Value, 1.0);
			constants.RoughnessMultiplier = Registry.get<GRComponents::RoughnessMultiplier>(ent).Value;
			constants.Metallic = Registry.get<GRComponents::MetallicOverride>(ent).Value;
			constants.HeightScale = Registry.get<GRComponents::DisplacementScale>(ent).Value;

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