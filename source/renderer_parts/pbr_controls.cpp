#include "pch.hpp"
#include "renderer.hpp"

entt::entity VulkanBase::AddMesh(const std::string& mesh_path)
{
	entt::entity ent = registry.create();

	PBRObject& gro = registry.emplace_or_replace<PBRObject>(ent);
	registry.emplace_or_replace<GRComponents::Transform>(ent);
	registry.emplace_or_replace<GRComponents::Color>(ent);
	registry.emplace_or_replace<GRComponents::RoughnessMultiplier>(ent);
	registry.emplace_or_replace<GRComponents::MetallicOverride>(ent);
	registry.emplace_or_replace<GRComponents::DisplacementScale>(ent);

	registry.emplace_or_replace<GRComponents::AlbedoMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::NormalMap>(ent, defaultNormal, &gro.dirty);
	registry.emplace_or_replace<GRComponents::RoughnessMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::MetallicMap>(ent, defaultBlack, &gro.dirty);
	registry.emplace_or_replace<GRComponents::AmbientMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::DisplacementMap>(ent, defaultWhite, &gro.dirty);

	gro.mesh = mesh_path != "" ? GRFile::ImportMesh(Scope, mesh_path.c_str())
		: GRShape::Cube().Generate(Scope);

	gro.descriptorSet = create_pbr_set(*defaultWhite, *defaultNormal, *defaultWhite, *defaultBlack, *defaultWhite, *defaultWhite);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);

	return ent;
}

entt::entity VulkanBase::AddShape(const GRShape::Shape& descriptor)
{
	entt::entity ent = registry.create();
	PBRObject& gro = registry.emplace_or_replace<PBRObject>(ent);
	registry.emplace_or_replace<GRComponents::Transform>(ent);
	registry.emplace_or_replace<GRComponents::Color>(ent);
	registry.emplace_or_replace<GRComponents::RoughnessMultiplier>(ent);
	registry.emplace_or_replace<GRComponents::MetallicOverride>(ent);
	registry.emplace_or_replace<GRComponents::DisplacementScale>(ent);

	registry.emplace_or_replace<GRComponents::AlbedoMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::NormalMap>(ent, defaultNormal, &gro.dirty);
	registry.emplace_or_replace<GRComponents::RoughnessMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::MetallicMap>(ent, defaultBlack, &gro.dirty);
	registry.emplace_or_replace<GRComponents::AmbientMap>(ent, defaultWhite, &gro.dirty);
	registry.emplace_or_replace<GRComponents::DisplacementMap>(ent, defaultWhite, &gro.dirty);

	gro.mesh = descriptor.Generate(Scope);
	gro.descriptorSet = create_pbr_set(*defaultWhite, *defaultNormal, *defaultWhite, *defaultBlack, *defaultWhite, *defaultWhite);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);

	return ent;
}

void VulkanBase::update_pipeline(entt::entity ent)
{
	VulkanImage* albedo = static_cast<VulkanImage*>(registry.get<GRComponents::AlbedoMap>(ent).Get().get());
	VulkanImage* normal = static_cast<VulkanImage*>(registry.get<GRComponents::NormalMap>(ent).Get().get());
	VulkanImage* roughness = static_cast<VulkanImage*>(registry.get<GRComponents::RoughnessMap>(ent).Get().get());
	VulkanImage* metallic = static_cast<VulkanImage*>(registry.get<GRComponents::MetallicMap>(ent).Get().get());
	VulkanImage* ambient = static_cast<VulkanImage*>(registry.get<GRComponents::AmbientMap>(ent).Get().get());
	VulkanImage* displacement = static_cast<VulkanImage*>(registry.get<GRComponents::DisplacementMap>(ent).Get().get());

	PBRObject& gro = registry.get<PBRObject>(ent);
	gro.descriptorSet = create_pbr_set(*albedo, *normal, *roughness, *metallic, *ambient, *displacement);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);
	gro.dirty = false;
}

TAuto<DescriptorSet> VulkanBase::create_pbr_set(const VulkanImage& albedo
	, const VulkanImage& normal
	, const VulkanImage& roughness
	, const VulkanImage& metallic
	, const VulkanImage& ao
	, const VulkanImage& displacement)
{
	return DescriptorSetDescriptor()
		.AddUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *ubo)
		.AddUniformBuffer(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *view)
		.AddImageSampler(2, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *Transmittance)
		.AddImageSampler(3, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *IrradianceLUT)
		.AddImageSampler(4, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *ScatteringLUT)
		.AddImageSampler(5, VK_SHADER_STAGE_FRAGMENT_BIT, albedo)
		.AddImageSampler(6, VK_SHADER_STAGE_FRAGMENT_BIT, normal)
		.AddImageSampler(7, VK_SHADER_STAGE_FRAGMENT_BIT, roughness)
		.AddImageSampler(8, VK_SHADER_STAGE_FRAGMENT_BIT, metallic)
		.AddImageSampler(9, VK_SHADER_STAGE_FRAGMENT_BIT, ao)
		.AddImageSampler(10, VK_SHADER_STAGE_FRAGMENT_BIT, displacement)
		.Allocate(Scope);
}

TAuto<Pipeline> VulkanBase::create_pbr_pipeline(const DescriptorSet& set)
{
	auto vertAttributes = Vertex::getAttributeDescriptions();
	auto vertBindings = Vertex::getBindingDescription();

	return GraphicsPipelineDescriptor()
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.SetVertexInputBindings(1, &vertBindings)
		.SetVertexAttributeBindings(vertAttributes.size(), vertAttributes.data())
		.SetShaderStage("default_vert", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("default_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(set.GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PBRConstants::World) })
		.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PBRConstants::World),  sizeof(PBRConstants) - sizeof(PBRConstants::World) })
		.Construct(Scope);
};
