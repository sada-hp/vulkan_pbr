#include "pch.hpp"
#include "renderer.hpp"

#define WRAPL(i) (i == 0 ? m_ResourceCount : i) - 1
#define WRAPR(i) i == m_ResourceCount - 1 ? 0 : i + 1

VkBool32 VulkanBase::atmosphere_precompute()
{
	VulkanTexture DeltaE{};
	VulkanTexture DeltaSR{};
	VulkanTexture DeltaSM{};
	VulkanTexture DeltaJ{};

	std::unique_ptr<DescriptorSet> TrDSO;
	std::unique_ptr<DescriptorSet> DeltaEDSO;
	std::unique_ptr<DescriptorSet> DeltaSRSMDSO;
	std::unique_ptr<DescriptorSet> SingleScatterDSO;

	std::unique_ptr<DescriptorSet> DeltaJDSO;
	std::unique_ptr<DescriptorSet> DeltaEnDSO;
	std::unique_ptr<DescriptorSet> DeltaSDSO;
	std::unique_ptr<DescriptorSet> AddEDSO;
	std::unique_ptr<DescriptorSet> AddSDSO;

	std::unique_ptr<ComputePipeline> GenTrLUT;
	std::unique_ptr<ComputePipeline> GenDeltaELUT;
	std::unique_ptr<ComputePipeline> GenDeltaSRSMLUT;
	std::unique_ptr<ComputePipeline> GenSingleScatterLUT;
	std::unique_ptr<ComputePipeline> GenDeltaJLUT;
	std::unique_ptr<ComputePipeline> GenDeltaEnLUT;
	std::unique_ptr<ComputePipeline> GenDeltaSLUT;
	std::unique_ptr<ComputePipeline> AddE;
	std::unique_ptr<ComputePipeline> AddS;

	VkImageSubresourceRange subRes{};
	subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subRes.baseArrayLayer = 0;
	subRes.baseMipLevel = 0;
	subRes.layerCount = 1;
	subRes.levelCount = 1;

	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.arrayLayers = 1;
	imageCI.extent = { 256, 64, 1u };
	imageCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageCI.mipLevels = 1;
	imageCI.flags = 0;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCI.imageType = VK_IMAGE_TYPE_2D;

	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.subresourceRange = subRes;

	VmaAllocationCreateInfo imageAlloc{};
	imageAlloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkSampler ImageSampler = m_Scope.GetSampler(ESamplerType::PointClamp, 1);
	VkSampler ImageSampler2 = m_Scope.GetSampler(ESamplerType::BillinearClamp, 1);

	m_TransmittanceLUT.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	m_TransmittanceLUT.View = std::make_unique<VulkanImageView>(m_Scope, *m_TransmittanceLUT.Image);

	TrDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView())
		.Allocate(m_Scope);

	GenTrLUT = ComputePipelineDescriptor()
		.SetShaderName("transmittance_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(TrDSO->GetLayout())
		.Construct(m_Scope);

	imageCI.extent = { 64u, 16u, 1u };
	DeltaE.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	DeltaE.View = std::make_unique<VulkanImageView>(m_Scope, *DeltaE.Image);

	m_IrradianceLUT.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	m_IrradianceLUT.View = std::make_unique<VulkanImageView>(m_Scope, *m_IrradianceLUT.Image);

	DeltaEDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	GenDeltaELUT = ComputePipelineDescriptor()
		.SetShaderName("deltaE_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaEDSO->GetLayout())
		.Construct(m_Scope);

	imageCI.imageType = VK_IMAGE_TYPE_3D;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_3D;
	imageCI.extent = { 256u, 128u, 32u };
	DeltaSR.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	DeltaSR.View = std::make_unique<VulkanImageView>(m_Scope, *DeltaSR.Image);

	DeltaSM.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	DeltaSM.View = std::make_unique<VulkanImageView>(m_Scope, *DeltaSM.Image);

	DeltaSRSMDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView())
		.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSM.View->GetImageView())
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), ImageSampler2)
		.Allocate(m_Scope);

	GenDeltaSRSMLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaSRSM_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaSRSMDSO->GetLayout())
		.Construct(m_Scope);

	// imageCI.mipLevels = static_cast<uint32_t>(std::floor(std::log2(256u))) + 1;
	m_ScatteringLUT.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	m_ScatteringLUT.View = std::make_unique<VulkanImageView>(m_Scope, *m_ScatteringLUT.Image);
	// imageCI.mipLevels = 1;

	SingleScatterDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView(), ImageSampler2)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSM.View->GetImageView(), ImageSampler2)
		.Allocate(m_Scope);

	GenSingleScatterLUT = ComputePipelineDescriptor()
		.SetShaderName("singleScattering_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(SingleScatterDSO->GetLayout())
		.Construct(m_Scope);

	DeltaJ.Image = std::make_unique<VulkanImage>(m_Scope, imageCI, imageAlloc);
	DeltaJ.View = std::make_unique<VulkanImageView>(m_Scope, *DeltaJ.Image);

	DeltaJDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, DeltaJ.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), ImageSampler)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView(), ImageSampler)
		.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView(), ImageSampler)
		.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSM.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	GenDeltaJLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaJ_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaJDSO->GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int) })
		.Construct(m_Scope);

	DeltaEnDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, DeltaE.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView(), ImageSampler)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSM.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	GenDeltaEnLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaEn_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaEnDSO->GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int) })
		.Construct(m_Scope);

	DeltaSDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), ImageSampler)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, DeltaJ.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	GenDeltaSLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaS_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(DeltaSDSO->GetLayout())
		.Construct(m_Scope);

	AddEDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaE.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	AddE = ComputePipelineDescriptor()
		.SetShaderName("addE_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(AddEDSO->GetLayout())
		.Construct(m_Scope);

	AddSDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView())
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, DeltaSR.View->GetImageView(), ImageSampler)
		.Allocate(m_Scope);

	AddS = ComputePipelineDescriptor()
		.SetShaderName("addS_comp")
		.AddSpecializationConstant(0, Rg * 1e-3f)
		.AddSpecializationConstant(1, Rt * 1e-3f)
		.AddDescriptorLayout(AddSDSO->GetLayout())
		.Construct(m_Scope);

	VkCommandBuffer cmd;
	const Queue& Queue = m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT);

	Queue.AllocateCommandBuffers(1, &cmd);
	VkCommandBufferBeginInfo cmdBegin{};
	cmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	Queue.AllocateCommandBuffers2(1, &cmd);
	vkBeginCommandBuffer(cmd, &cmdBegin);

	GenTrLUT->BindPipeline(cmd);
	TrDSO->BindSet(0, cmd, *GenTrLUT);
	vkCmdDispatch(cmd, m_TransmittanceLUT.Image->GetExtent().width / 8u + uint32_t(m_TransmittanceLUT.Image->GetExtent().width % 8u > 0)
		, m_TransmittanceLUT.Image->GetExtent().height / 8u + uint32_t(m_TransmittanceLUT.Image->GetExtent().height % 8u > 0)
		, 1u);

	m_TransmittanceLUT.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

	GenDeltaELUT->BindPipeline(cmd);
	DeltaEDSO->BindSet(0, cmd, *GenDeltaELUT);
	vkCmdDispatch(cmd, m_IrradianceLUT.Image->GetExtent().width / 8u + uint32_t(m_IrradianceLUT.Image->GetExtent().width % 8u > 0),
		m_IrradianceLUT.Image->GetExtent().height / 8u + uint32_t(m_IrradianceLUT.Image->GetExtent().height % 8u > 0),
		1u);

	GenDeltaSRSMLUT->BindPipeline(cmd);
	DeltaSRSMDSO->BindSet(0, cmd, *GenDeltaSRSMLUT);
	vkCmdDispatch(cmd, DeltaSR.Image->GetExtent().width / 4u + uint32_t(DeltaSR.Image->GetExtent().width % 4u > 0),
		DeltaSR.Image->GetExtent().height / 4u + uint32_t(DeltaSR.Image->GetExtent().height % 4u > 0),
		DeltaSR.Image->GetExtent().depth / 4u + uint32_t(DeltaSR.Image->GetExtent().depth % 4u > 0));

	DeltaSM.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
	DeltaSR.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

	GenSingleScatterLUT->BindPipeline(cmd);
	SingleScatterDSO->BindSet(0, cmd, *GenSingleScatterLUT);
	vkCmdDispatch(cmd, m_ScatteringLUT.Image->GetExtent().width / 4u + uint32_t(m_ScatteringLUT.Image->GetExtent().width % 4u > 0),
		m_ScatteringLUT.Image->GetExtent().height / 4u + uint32_t(m_ScatteringLUT.Image->GetExtent().height % 4u > 0),
		m_ScatteringLUT.Image->GetExtent().depth / 4u + uint32_t(m_ScatteringLUT.Image->GetExtent().depth % 4u > 0));

	::EndCommandBuffer(cmd);
	Queue.Submit(cmd)
		.Wait();

	for (uint32_t Sample = 2; Sample <= 5; Sample++)
	{
		Queue.Wait();
		vkBeginCommandBuffer(cmd, &cmdBegin);

		DeltaJ.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		GenDeltaJLUT->PushConstants(cmd, &Sample, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		GenDeltaJLUT->BindPipeline(cmd);

		m_IrradianceLUT.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		DeltaJDSO->BindSet(0, cmd, *GenDeltaJLUT);
		vkCmdDispatch(cmd, DeltaJ.Image->GetExtent().width / 4u + uint32_t(DeltaJ.Image->GetExtent().width % 4u > 0),
			DeltaJ.Image->GetExtent().height / 4u + uint32_t(DeltaJ.Image->GetExtent().height % 4u > 0),
			DeltaJ.Image->GetExtent().depth / 4u + uint32_t(DeltaJ.Image->GetExtent().depth % 4u > 0));

		DeltaE.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		GenDeltaEnLUT->PushConstants(cmd, &Sample, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		GenDeltaEnLUT->BindPipeline(cmd);
		DeltaEnDSO->BindSet(0, cmd, *GenDeltaEnLUT);
		vkCmdDispatch(cmd, DeltaE.Image->GetExtent().width / 8u + uint32_t(DeltaE.Image->GetExtent().width % 8u > 0),
			DeltaE.Image->GetExtent().height / 8u + uint32_t(DeltaE.Image->GetExtent().height % 8u > 0),
			1u);

		DeltaSR.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);
		DeltaJ.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		GenDeltaSLUT->BindPipeline(cmd);
		DeltaSDSO->BindSet(0, cmd, *GenDeltaSLUT);
		vkCmdDispatch(cmd, DeltaSR.Image->GetExtent().width / 4u + uint32_t(DeltaSR.Image->GetExtent().width % 4u > 0),
			DeltaSR.Image->GetExtent().height / 4u + uint32_t(DeltaSR.Image->GetExtent().height % 4u > 0),
			DeltaSR.Image->GetExtent().depth / 4u + uint32_t(DeltaSR.Image->GetExtent().depth % 4u > 0));

		DeltaE.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);
		m_IrradianceLUT.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_COMPUTE_BIT);

		AddE->BindPipeline(cmd);
		AddEDSO->BindSet(0, cmd, *AddE);
		vkCmdDispatch(cmd, m_IrradianceLUT.Image->GetExtent().width / 8u + uint32_t(m_IrradianceLUT.Image->GetExtent().width % 8u > 0),
			m_IrradianceLUT.Image->GetExtent().height / 8u + uint32_t(m_IrradianceLUT.Image->GetExtent().height % 8u > 0),
			1u);

		DeltaSR.Image->TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_COMPUTE_BIT);

		AddS->BindPipeline(cmd);
		AddSDSO->BindSet(0, cmd, *AddS);
		vkCmdDispatch(cmd, DeltaSR.Image->GetExtent().width / 4u + uint32_t(DeltaSR.Image->GetExtent().width % 4u > 0),
			DeltaSR.Image->GetExtent().height / 4u + uint32_t(DeltaSR.Image->GetExtent().height % 4u > 0),
			DeltaSR.Image->GetExtent().depth / 4u + uint32_t(DeltaSR.Image->GetExtent().depth % 4u > 0));

		vkEndCommandBuffer(cmd);
		Queue.Submit(cmd);
	}

	Queue.Wait()
		.FreeCommandBuffers(1, &cmd);

	m_TransmittanceLUT.Image->GenerateMipMaps();

	m_TransmittanceLUT.Image->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ScatteringLUT.Image->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_IrradianceLUT.Image->TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	return 1;
}