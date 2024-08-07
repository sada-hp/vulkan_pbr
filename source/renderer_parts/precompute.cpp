#include "pch.hpp"
#include "renderer.hpp"

VkBool32 VulkanBase::atmosphere_precompute()
{
	TAuto<VulkanImage> DeltaE;
	TAuto<VulkanImage> DeltaSR;
	TAuto<VulkanImage> DeltaSM;
	TAuto<VulkanImage> DeltaJ;

	TAuto<DescriptorSet> TrDSO;
	TAuto<DescriptorSet> DeltaEDSO;
	TAuto<DescriptorSet> DeltaSRSMDSO;
	TAuto<DescriptorSet> SingleScatterDSO;

	TAuto<DescriptorSet> DeltaJDSO;
	TAuto<DescriptorSet> DeltaEnDSO;
	TAuto<DescriptorSet> DeltaSDSO;
	TAuto<DescriptorSet> AddEDSO;
	TAuto<DescriptorSet> AddSDSO;

	TAuto<Pipeline> GenTrLUT;
	TAuto<Pipeline> GenDeltaELUT;
	TAuto<Pipeline> GenDeltaSRSMLUT;
	TAuto<Pipeline> GenSingleScatterLUT;
	TAuto<Pipeline> GenDeltaJLUT;
	TAuto<Pipeline> GenDeltaEnLUT;
	TAuto<Pipeline> GenDeltaSLUT;
	TAuto<Pipeline> AddE;
	TAuto<Pipeline> AddS;

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
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCI.mipLevels = 1;
	imageCI.flags = 0;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCI.imageType = VK_IMAGE_TYPE_2D;

	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.subresourceRange = subRes;

	VmaAllocationCreateInfo imageAlloc{};
	imageAlloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	m_TransmittanceLUT = std::make_unique<VulkanImage>(m_Scope);
	m_TransmittanceLUT->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::PointClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	TrDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_TransmittanceLUT)
		.Allocate(m_Scope);

	GenTrLUT = ComputePipelineDescriptor()
		.SetShaderName("transmittance_comp")
		.AddDescriptorLayout(TrDSO->GetLayout())
		.Construct(m_Scope);

	imageCI.extent = { 64u, 16u, 1u };
	DeltaE = std::make_unique<VulkanImage>(m_Scope);
	DeltaE->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::PointClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	DeltaEDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaE)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *m_TransmittanceLUT)
		.Allocate(m_Scope);

	GenDeltaELUT = ComputePipelineDescriptor()
		.SetShaderName("deltaE_comp")
		.AddDescriptorLayout(DeltaEDSO->GetLayout())
		.Construct(m_Scope);

	m_IrradianceLUT = std::make_unique<VulkanImage>(m_Scope);
	m_IrradianceLUT->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::PointClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	imageCI.imageType = VK_IMAGE_TYPE_3D;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_3D;
	imageCI.extent = { 256u, 128u, 32u };
	DeltaSR = std::make_unique<VulkanImage>(m_Scope);
	DeltaSR->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::PointClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	DeltaSM = std::make_unique<VulkanImage>(m_Scope);
	DeltaSM->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::PointClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	DeltaSRSMDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSM)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, *m_TransmittanceLUT)
		.Allocate(m_Scope);

	GenDeltaSRSMLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaSRSM_comp")
		.AddDescriptorLayout(DeltaSRSMDSO->GetLayout())
		.Construct(m_Scope);

	m_ScatteringLUT = std::make_unique<VulkanImage>(m_Scope);
	m_ScatteringLUT->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::PointClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	SingleScatterDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_ScatteringLUT)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSM)
		.Allocate(m_Scope);

	GenSingleScatterLUT = ComputePipelineDescriptor()
		.SetShaderName("singleScattering_comp")
		.AddDescriptorLayout(SingleScatterDSO->GetLayout())
		.Construct(m_Scope);

	DeltaJ = std::make_unique<VulkanImage>(m_Scope);
	DeltaJ->CreateImage(imageCI, imageAlloc)
		.CreateImageView(imageViewCI)
		.CreateSampler(ESamplerType::PointClamp)
		.TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

	DeltaJDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaJ)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *m_TransmittanceLUT)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaE)
		.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSM)
		.Allocate(m_Scope);

	GenDeltaJLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaJ_comp")
		.AddDescriptorLayout(DeltaJDSO->GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int) })
		.Construct(m_Scope);

	DeltaEnDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaE)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSM)
		.Allocate(m_Scope);

	GenDeltaEnLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaEn_comp")
		.AddDescriptorLayout(DeltaEnDSO->GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int) })
		.Construct(m_Scope);

	DeltaSDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *m_TransmittanceLUT)
		.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaJ)
		.Allocate(m_Scope);

	GenDeltaSLUT = ComputePipelineDescriptor()
		.SetShaderName("deltaS_comp")
		.AddDescriptorLayout(DeltaSDSO->GetLayout())
		.Construct(m_Scope);

	AddEDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_IrradianceLUT)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaE)
		.Allocate(m_Scope);

	AddE = ComputePipelineDescriptor()
		.SetShaderName("addE_comp")
		.AddDescriptorLayout(AddEDSO->GetLayout())
		.Construct(m_Scope);

	AddSDSO = DescriptorSetDescriptor()
		.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_ScatteringLUT)
		.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, *DeltaSR)
		.Allocate(m_Scope);

	AddS = ComputePipelineDescriptor()
		.SetShaderName("addS_comp")
		.AddDescriptorLayout(AddSDSO->GetLayout())
		.Construct(m_Scope);

	VkCommandBuffer cmd;
	const Queue& Queue = m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_COMPUTE_BIT;
	barrier.dstQueueFamilyIndex = VK_QUEUE_COMPUTE_BIT;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	Queue.AllocateCommandBuffers(1, &cmd);
	::BeginOneTimeSubmitCmd(cmd);

	GenTrLUT->BindPipeline(cmd);
	TrDSO->BindSet(0, cmd, *GenTrLUT);
	vkCmdDispatch(cmd, m_TransmittanceLUT->GetExtent().width / 8u + uint32_t(m_TransmittanceLUT->GetExtent().width % 8u > 0)
		, m_TransmittanceLUT->GetExtent().height / 8u + uint32_t(m_TransmittanceLUT->GetExtent().height % 8u > 0)
		, 1u);

	barrier.image = m_TransmittanceLUT->GetImage();
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	GenDeltaELUT->BindPipeline(cmd);
	DeltaEDSO->BindSet(0, cmd, *GenDeltaELUT);
	vkCmdDispatch(cmd, DeltaE->GetExtent().width / 8u + uint32_t(DeltaE->GetExtent().width % 8u > 0),
		DeltaE->GetExtent().height / 8u + uint32_t(DeltaE->GetExtent().height % 8u > 0),
		1u);

	barrier.image = DeltaE->GetImage();
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	GenDeltaSRSMLUT->BindPipeline(cmd);
	DeltaSRSMDSO->BindSet(0, cmd, *GenDeltaSRSMLUT);
	vkCmdDispatch(cmd, DeltaSR->GetExtent().width / 4u + uint32_t(DeltaSR->GetExtent().width % 4u > 0),
		DeltaSR->GetExtent().height / 4u + uint32_t(DeltaSR->GetExtent().height % 4u > 0),
		DeltaSR->GetExtent().depth / 4u + uint32_t(DeltaSR->GetExtent().depth % 4u > 0));

	barrier.image = DeltaSM->GetImage();
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	GenSingleScatterLUT->BindPipeline(cmd);
	SingleScatterDSO->BindSet(0, cmd, *GenSingleScatterLUT);
	vkCmdDispatch(cmd, m_ScatteringLUT->GetExtent().width / 4u + uint32_t(m_ScatteringLUT->GetExtent().width % 4u > 0),
		m_ScatteringLUT->GetExtent().height / 4u + uint32_t(m_ScatteringLUT->GetExtent().height % 4u > 0),
		m_ScatteringLUT->GetExtent().depth / 4u + uint32_t(m_ScatteringLUT->GetExtent().depth % 4u > 0));

	for (int Sample = 2; Sample <= 15; ++Sample)
	{
		GenDeltaJLUT->PushConstants(cmd, &Sample, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		GenDeltaJLUT->BindPipeline(cmd);
		DeltaJDSO->BindSet(0, cmd, *GenDeltaJLUT);
		vkCmdDispatch(cmd, DeltaJ->GetExtent().width / 4u + uint32_t(DeltaJ->GetExtent().width % 4u > 0),
			DeltaJ->GetExtent().height / 4u + uint32_t(DeltaJ->GetExtent().height % 4u > 0),
			DeltaJ->GetExtent().depth / 4u + uint32_t(DeltaJ->GetExtent().depth % 4u > 0));

		GenDeltaEnLUT->PushConstants(cmd, &Sample, sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
		GenDeltaEnLUT->BindPipeline(cmd);
		DeltaEnDSO->BindSet(0, cmd, *GenDeltaEnLUT);
		vkCmdDispatch(cmd, DeltaE->GetExtent().width / 8u + uint32_t(DeltaE->GetExtent().width % 8u > 0),
			DeltaE->GetExtent().height / 8u + uint32_t(DeltaE->GetExtent().height % 8u > 0),
			1u);

		GenDeltaSLUT->BindPipeline(cmd);
		DeltaSDSO->BindSet(0, cmd, *GenDeltaSLUT);
		vkCmdDispatch(cmd, DeltaSR->GetExtent().width / 4u + uint32_t(DeltaSR->GetExtent().width % 4u > 0),
			DeltaSR->GetExtent().height / 4u + uint32_t(DeltaSR->GetExtent().height % 4u > 0),
			DeltaSR->GetExtent().depth / 4u + uint32_t(DeltaSR->GetExtent().depth % 4u > 0));

		AddE->BindPipeline(cmd);
		AddEDSO->BindSet(0, cmd, *AddE);
		vkCmdDispatch(cmd, m_IrradianceLUT->GetExtent().width / 8u + uint32_t(m_IrradianceLUT->GetExtent().width % 8u > 0),
			m_IrradianceLUT->GetExtent().height / 8u + uint32_t(m_IrradianceLUT->GetExtent().height % 8u > 0),
			1u);

		AddS->BindPipeline(cmd);
		AddSDSO->BindSet(0, cmd, *AddS);
		vkCmdDispatch(cmd, DeltaSR->GetExtent().width / 4u + uint32_t(DeltaSR->GetExtent().width % 4u > 0),
			DeltaSR->GetExtent().height / 4u + uint32_t(DeltaSR->GetExtent().height % 4u > 0),
			DeltaSR->GetExtent().depth / 4u + uint32_t(DeltaSR->GetExtent().depth % 4u > 0));
	}

	::EndCommandBuffer(cmd);
	Queue.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	m_TransmittanceLUT->CreateSampler(ESamplerType::BillinearClamp);
	m_ScatteringLUT->CreateSampler(ESamplerType::BillinearClamp);
	m_IrradianceLUT->CreateSampler(ESamplerType::BillinearClamp);

	m_Atmospherics = std::make_unique<GraphicsObject>();
	m_Atmospherics->descriptorSet = DescriptorSetDescriptor()
		.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, *m_ScatteringLUT)
		.Allocate(m_Scope);

	m_Atmospherics->pipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("background_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetCullMode(VK_CULL_MODE_NONE)
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(m_Atmospherics->descriptorSet->GetLayout())
		.Construct(m_Scope);

	return 1;
}

VkBool32 VulkanBase::volumetric_precompute()
{
	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	VkBufferCreateInfo cloudInfo{};
	cloudInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	cloudInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	cloudInfo.size = sizeof(CloudLayerProfile);
	cloudInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	m_CloudLayer = std::make_unique<Buffer>(m_Scope, cloudInfo, allocCreateInfo);

	CloudLayerProfile defaultClouds{};
	m_CloudLayer->Update(&defaultClouds, sizeof(CloudLayerProfile));

	m_VolumeShape = GRNoise::GenerateCloudShapeNoise(m_Scope, { 128u, 128u, 128u }, 4u, 4u);
	m_VolumeDetail = GRNoise::GenerateCloudDetailNoise(m_Scope, { 32u, 32u, 32u }, 6u, 3u);

	m_Volumetrics = std::make_unique<GraphicsObject>();
	m_Volumetrics->descriptorSet = DescriptorSetDescriptor()
		.AddUniformBuffer(1, VK_SHADER_STAGE_FRAGMENT_BIT, *m_CloudLayer)
		.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, *m_VolumeShape)
		.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, *m_VolumeDetail)
		.AddImageSampler(4, VK_SHADER_STAGE_FRAGMENT_BIT, *m_TransmittanceLUT)
		.AddImageSampler(5, VK_SHADER_STAGE_FRAGMENT_BIT, *m_IrradianceLUT)
		.AddImageSampler(6, VK_SHADER_STAGE_FRAGMENT_BIT, *m_ScatteringLUT)
		.Allocate(m_Scope);

	VkPipelineColorBlendAttachmentState blendState{};
	blendState.blendEnable = true;
	blendState.colorBlendOp = VK_BLEND_OP_ADD;
	blendState.alphaBlendOp = VK_BLEND_OP_ADD;
	blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendState.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;

	m_Volumetrics->pipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("volumetric_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetBlendAttachments(1, &blendState)
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(m_Volumetrics->descriptorSet->GetLayout())
		.SetCullMode(VK_CULL_MODE_FRONT_BIT)
		.Construct(m_Scope);
	
	return 1;
}