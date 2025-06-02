#include "pch.hpp"
#include "renderer.hpp"

#define WRAPL(i) (i == 0 ? m_ResourceCount : i) - 1
#define WRAPR(i) i == m_ResourceCount - 1 ? 0 : i + 1

VkBool32 VulkanBase::volumetric_precompute()
{
	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkBufferCreateInfo cloudInfo{};
	cloudInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	cloudInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	cloudInfo.size = sizeof(CloudParameters);
	cloudInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	m_CloudLayer = std::make_unique<Buffer>(m_Scope, cloudInfo, allocCreateInfo);

	SetCloudLayerSettings(CloudLayerProfile());

	m_VolumeShape.Image = GRNoise::GenerateCloudShapeNoise(m_Scope, { 128u, 128u, 128u }, 1u, 4u);
	m_VolumeShape.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeShape.Image);

	m_VolumeDetail.Image = GRNoise::GenerateCloudDetailNoise(m_Scope, { 64u, 64u, 64u }, 2u, 16u);
	m_VolumeDetail.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeDetail.Image);

	m_VolumeWeather.Image = GRNoise::GenerateWorleyPerlin(m_Scope, { 256u, 256u, 1u }, 16u, 8u, 8u);
	m_VolumeWeather.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeWeather.Image);

	m_VolumeWeatherCube.Image = GRNoise::GenerateWeatherCube(m_Scope, { 256u, 256u, 1u }, 16u, 8u, 8u);

	m_VolumeWeatherCube.Image->flags &= ~VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	m_VolumeWeatherCube.View = std::make_unique<VulkanImageView>(m_Scope, *m_VolumeWeatherCube.Image);

	VkSampler SamplerClamp = m_Scope.GetSampler(ESamplerType::BillinearClamp, 1);

	// m_Volumetrics = std::make_unique<GraphicsObject>();
	m_VolumetricsDescriptor = DescriptorSetDescriptor()
		.AddUniformBuffer(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_CloudLayer)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_VolumeShape.View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, m_VolumeShape.View->GetSubresourceRange().levelCount))
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_VolumeDetail.View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, m_VolumeDetail.View->GetSubresourceRange().levelCount))
		.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, m_VolumeWeather.View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, m_VolumeWeather.View->GetSubresourceRange().levelCount))
		.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(5, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(6, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView(), SamplerClamp)
		.AddImageSampler(7, VK_SHADER_STAGE_COMPUTE_BIT, m_VolumeWeatherCube.View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, m_VolumeWeatherCube.View->GetSubresourceRange().levelCount))
		.Allocate(m_Scope);

	VkDescriptorSetLayout dummy;
	CreateDescriptorLayout(m_Scope.GetDevice(), { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE , VK_DESCRIPTOR_TYPE_STORAGE_IMAGE , VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER , VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, VK_SHADER_STAGE_COMPUTE_BIT, &dummy);

	VkPushConstantRange ConstantOrder{};
	ConstantOrder.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	ConstantOrder.size = sizeof(int);

	ComputePipelineDescriptor VolumetricPSO{};
	VolumetricPSO.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(m_VolumetricsDescriptor->GetLayout())
		.AddDescriptorLayout(dummy)
		.AddPushConstant(ConstantOrder)
		.AddSpecializationConstant(0, Rg)
		.AddSpecializationConstant(1, Rt);

	m_VolumetricsAbovePipeline = VolumetricPSO.SetShaderName("volumetric_above_comp")
		.Construct(m_Scope);

	m_VolumetricsBetweenPipeline = VolumetricPSO.SetShaderName("volumetric_between_comp")
		.Construct(m_Scope);

	m_VolumetricsUnderPipeline = VolumetricPSO.SetShaderName("volumetric_under_comp")
		.Construct(m_Scope);

	m_VolumetricsComposePipeline = ComputePipelineDescriptor()
		.SetShaderName("volumetric_compose_comp")
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(dummy)
		.AddPushConstant(ConstantOrder)
		.Construct(m_Scope);

	vkDestroyDescriptorSetLayout(m_Scope.GetDevice(), dummy, VK_NULL_HANDLE);

	return 1;
}