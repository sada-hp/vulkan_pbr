#include "pch.hpp"
#include "renderer.hpp"

entt::entity VulkanBase::AddMesh(const std::string& mesh_path)
{
	entt::entity ent = m_Registry.create();

	PBRObject& gro = m_Registry.emplace_or_replace<PBRObject>(ent);
	m_Registry.emplace_or_replace<GRComponents::Transform<float>>(ent);
	m_Registry.emplace_or_replace<GRComponents::RGBColor>(ent);
	m_Registry.emplace_or_replace<GRComponents::RoughnessMultiplier>(ent);
	m_Registry.emplace_or_replace<GRComponents::MetallicOverride>(ent);
	m_Registry.emplace_or_replace<GRComponents::DisplacementScale>(ent);

	m_Registry.emplace_or_replace<GRComponents::AlbedoMap>(ent, m_DefaultWhite, &gro.dirty);
	m_Registry.emplace_or_replace<GRComponents::NormalDisplacementMap>(ent, m_DefaultNormal, &gro.dirty);
	m_Registry.emplace_or_replace<GRComponents::AORoughnessMetallicMap>(ent, m_DefaultWhite, &gro.dirty);

	gro.mesh = mesh_path != "" ? GRVkFile::_importMesh(m_Scope, mesh_path.c_str())
		: GRShape::Cube().Generate(m_Scope);

	gro.descriptorSet = create_pbr_set(*m_DefaultWhite->View, *m_DefaultNormal->View, *m_DefaultARM->View);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);

	return ent;
}

entt::entity VulkanBase::AddShape(const GRShape::Shape& descriptor)
{
	entt::entity ent = m_Registry.create();
	PBRObject& gro = m_Registry.emplace_or_replace<PBRObject>(ent);
	m_Registry.emplace_or_replace<GRComponents::Transform<float>>(ent);
	m_Registry.emplace_or_replace<GRComponents::RGBColor>(ent);
	m_Registry.emplace_or_replace<GRComponents::RoughnessMultiplier>(ent);
	m_Registry.emplace_or_replace<GRComponents::MetallicOverride>(ent);
	m_Registry.emplace_or_replace<GRComponents::DisplacementScale>(ent);

	m_Registry.emplace_or_replace<GRComponents::AlbedoMap>(ent, m_DefaultWhite, &gro.dirty);
	m_Registry.emplace_or_replace<GRComponents::NormalDisplacementMap>(ent, m_DefaultNormal, &gro.dirty);
	m_Registry.emplace_or_replace<GRComponents::AORoughnessMetallicMap>(ent, m_DefaultWhite, &gro.dirty);

	gro.mesh = descriptor.Generate(m_Scope);
	gro.descriptorSet = create_pbr_set(*m_DefaultWhite->View, *m_DefaultNormal->View, *m_DefaultARM->View);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);

	return ent;
}

void VulkanBase::update_pipeline(entt::entity ent)
{
	VulkanTexture* albedo = static_cast<VulkanTexture*>(m_Registry.get<GRComponents::AlbedoMap>(ent).Get().get());
	VulkanTexture* nh = static_cast<VulkanTexture*>(m_Registry.get<GRComponents::NormalDisplacementMap>(ent).Get().get());
	VulkanTexture* arm = static_cast<VulkanTexture*>(m_Registry.get<GRComponents::AORoughnessMetallicMap>(ent).Get().get());

	PBRObject& gro = m_Registry.get<PBRObject>(ent);
	gro.descriptorSet = create_pbr_set(*albedo->View, *nh->View, *arm->View);
	gro.pipeline = create_pbr_pipeline(*gro.descriptorSet);
	gro.dirty = false;
}

TAuto<DescriptorSet> VulkanBase::create_pbr_set(const VulkanImageView& albedo
	, const VulkanImageView& nh
	, const VulkanImageView& arm)
{
	VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::BillinearRepeat);
	VkSampler SamplerClamp = m_Scope.GetSampler(ESamplerType::BillinearClamp);

	return DescriptorSetDescriptor()
		.AddImageSampler(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(2, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_IrradianceLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(3, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_ScatteringLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(4, VK_SHADER_STAGE_FRAGMENT_BIT, albedo.GetImageView(), SamplerRepeat)
		.AddImageSampler(5, VK_SHADER_STAGE_FRAGMENT_BIT, nh.GetImageView(), SamplerRepeat)
		.AddImageSampler(6, VK_SHADER_STAGE_FRAGMENT_BIT, arm.GetImageView(), SamplerRepeat)
		.Allocate(m_Scope);
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
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(set.GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PBRConstants::World) })
		.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PBRConstants::World),  sizeof(PBRConstants) - sizeof(PBRConstants::World) })
		.Construct(m_Scope);
}