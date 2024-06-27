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
	registry.emplace_or_replace<GRComponents::NormalDisplacementMap>(ent, defaultNormal, &gro.dirty);
	registry.emplace_or_replace<GRComponents::AORoughnessMetallicMap>(ent, defaultWhite, &gro.dirty);

	gro.mesh = mesh_path != "" ? GRVkFile::_importMesh(Scope, mesh_path.c_str())
		: GRShape::Cube().Generate(Scope);

	gro.descriptorSet = create_pbr_set(*defaultWhite, *defaultNormal, *defaultARM);
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
	registry.emplace_or_replace<GRComponents::NormalDisplacementMap>(ent, defaultNormal, &gro.dirty);
	registry.emplace_or_replace<GRComponents::AORoughnessMetallicMap>(ent, defaultWhite, &gro.dirty);

	gro.mesh = descriptor.Generate(Scope);
	gro.descriptorSet = create_pbr_set(*defaultWhite, *defaultNormal, *defaultARM);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);

	return ent;
}

void VulkanBase::update_pipeline(entt::entity ent)
{
	VulkanImage* albedo = static_cast<VulkanImage*>(registry.get<GRComponents::AlbedoMap>(ent).Get().get());
	VulkanImage* nh = static_cast<VulkanImage*>(registry.get<GRComponents::NormalDisplacementMap>(ent).Get().get());
	VulkanImage* arm = static_cast<VulkanImage*>(registry.get<GRComponents::AORoughnessMetallicMap>(ent).Get().get());

	PBRObject& gro = registry.get<PBRObject>(ent);
	gro.descriptorSet = create_pbr_set(*albedo, *nh, *arm);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);
	gro.dirty = false;
}

TAuto<DescriptorSet> VulkanBase::create_pbr_set(const VulkanImage& albedo
	, const VulkanImage& nh
	, const VulkanImage& arm)
{
	return DescriptorSetDescriptor()
		.AddImageSampler(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *Transmittance)
		.AddImageSampler(2, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *IrradianceLUT)
		.AddImageSampler(3, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, *ScatteringLUT)
		.AddImageSampler(4, VK_SHADER_STAGE_FRAGMENT_BIT, albedo)
		.AddImageSampler(5, VK_SHADER_STAGE_FRAGMENT_BIT, nh)
		.AddImageSampler(6, VK_SHADER_STAGE_FRAGMENT_BIT, arm)
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
		.AddDescriptorLayout(UBOSet[0]->GetLayout())
		.AddDescriptorLayout(set.GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PBRConstants::World) })
		.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PBRConstants::World),  sizeof(PBRConstants) - sizeof(PBRConstants::World) })
		.Construct(Scope);
};