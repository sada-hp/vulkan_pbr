#include "pch.hpp"
#include "renderer.hpp"

#define WRAPL(i) (i == 0 ? m_ResourceCount : i) - 1
#define WRAPR(i) i == m_ResourceCount - 1 ? 0 : i + 1

void VulkanBase::_drawTerrain(const PBRObject& gro, const PBRConstants& constants) const
{
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0)
		return;

	assert(m_InFrame, "Call BeginFrame first!");

	const VkCommandBuffer& cmd = m_DeferredSync[m_ResourceIndex].Commands;
	const VkDeviceSize offsets[] = { 0 };

	m_UBOSets[m_ResourceIndex]->BindSet(0, cmd, *gro.pipeline);
	m_TerrainDrawSet[m_ResourceIndex]->BindSet(1, cmd, *gro.pipeline);

	gro.pipeline->PushConstants(cmd, &constants.Offset, PBRConstants::VertexSize(), 0u, VK_SHADER_STAGE_VERTEX_BIT);
	gro.pipeline->BindPipeline(cmd);

	vkCmdBindVertexBuffers(cmd, 0, 1, &TerrainVBs[m_ResourceIndex]->GetBuffer(), offsets);
	vkCmdBindIndexBuffer(cmd, gro.mesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, gro.mesh->GetIndicesCount(), 1, 0, 0, 0);

	// grass
	if (glm::length(m_Camera.Transform.offset) < Rt)
	{
		m_GrassPipeline->BindPipeline(cmd);
		m_UBOSets[m_ResourceIndex]->BindSet(0, cmd, *m_GrassPipeline);
		m_GrassSet[m_ResourceIndex]->BindSet(1, cmd, *m_GrassPipeline);
		vkCmdDrawIndirectCount(m_DeferredSync[m_ResourceIndex].Commands, m_GrassIndirect[m_ResourceIndex]->GetBuffer(), 0, m_GrassIndirect[m_ResourceIndex]->GetBuffer(), m_GrassIndirect[m_ResourceIndex]->GetSize() - sizeof(uint32_t), m_TerrainLUT[0].Image->GetArrayLayers(), sizeof(VkDrawIndirectCommand));
	}

	vkCmdNextSubpass(m_DeferredSync[m_ResourceIndex].Commands, VK_SUBPASS_CONTENTS_INLINE);

	m_TerrainTexturingPipeline->PushConstants(cmd, &constants.Color, PBRConstants::FragmentSize(), 0.0, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_UBOSets[m_ResourceIndex]->BindSet(0, cmd, *m_TerrainTexturingPipeline);
	gro.descriptorSet->BindSet(1, cmd, *m_TerrainTexturingPipeline);
	m_SubpassDescriptors[m_ResourceIndex]->BindSet(2, cmd, *m_TerrainTexturingPipeline);
	m_TerrainTexturingPipeline->BindPipeline(cmd);
	vkCmdDraw(cmd, 3, 1, 0, 0);
}

void VulkanBase::_updateTerrain(entt::entity ent, entt::registry& registry) const
{
	VulkanTexture* albedo = static_cast<VulkanTexture*>(registry.get<GR::Components::AlbedoMap>(ent).Get().get());
	VulkanTexture* nh = static_cast<VulkanTexture*>(registry.get<GR::Components::NormalDisplacementMap>(ent).Get().get());
	VulkanTexture* arm = static_cast<VulkanTexture*>(registry.get<GR::Components::AORoughnessMetallicMapTransmittance>(ent).Get().get());

	PBRObject& gro = registry.get<PBRObject>(ent);
	gro.descriptorSet = create_terrain_set(*albedo->View, *nh->View, *arm->View);
	gro.dirty = false;
}

std::unique_ptr<DescriptorSet> VulkanBase::create_terrain_set(const VulkanImageView& albedo
	, const VulkanImageView& nh
	, const VulkanImageView& arm) const
{
	return DescriptorSetDescriptor()
		.AddImageSampler(0, VK_SHADER_STAGE_FRAGMENT_BIT, albedo.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, albedo.GetSubresourceRange().levelCount))
		.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, nh.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, nh.GetSubresourceRange().levelCount))
		.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, arm.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, arm.GetSubresourceRange().levelCount))
		// .AddImageSampler(4, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TerrainLUT.View->GetImageView(), SamplerRepeat)
		.Allocate(m_Scope);
}

std::unique_ptr<GraphicsPipeline> VulkanBase::create_terrain_pipeline(const DescriptorSet& set, const GR::Shapes::GeoClipmap& shape) const
{
	auto vertAttributes = TerrainVertex::getAttributeDescriptions();
	auto vertBindings = TerrainVertex::getBindingDescription();

	return GraphicsPipelineDescriptor()
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.SetBlendAttachments(3, nullptr)
		.SetVertexInputBindings(1, &vertBindings)
		.SetVertexAttributeBindings(vertAttributes.size(), vertAttributes.data())
		.SetShaderStage("terrain_vert", VK_SHADER_STAGE_VERTEX_BIT)
		// .SetShaderStage("terrain_geom", VK_SHADER_STAGE_GEOMETRY_BIT)
		.SetShaderStage("terrain_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		// .AddDescriptorLayout(set.GetLayout())
		.AddDescriptorLayout(m_TerrainDrawSet[0]->GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<uint32_t>(PBRConstants::VertexSize()) })
		.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(PBRConstants::VertexSize()), static_cast<uint32_t>(PBRConstants::FragmentSize()) })
		.AddSpecializationConstant(0, Rg, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(1, Rt, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(2, shape.m_Scale, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(3, shape.m_MinHeight, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(4, glm::max(shape.m_MaxHeight, shape.m_MinHeight + 1), VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(5, shape.m_NoiseSeed, VK_SHADER_STAGE_VERTEX_BIT)
		.AddSpecializationConstant(6, float(glm::ceil(float(shape.m_Rings) / 3.f) + 1.f), VK_SHADER_STAGE_VERTEX_BIT)
		.SetRenderPass(m_Scope.GetTerrainPass(), 0)
		// .SetPolygonMode(VK_POLYGON_MODE_LINE)
		.Construct(m_Scope);
}

VkBool32 VulkanBase::terrain_init(const Buffer& VB, const GR::Shapes::GeoClipmap& shape)
{
	std::vector<uint32_t> queueFamilies = FindDeviceQueues(m_Scope.GetPhysicalDevice(), { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT });
	std::sort(queueFamilies.begin(), queueFamilies.end());
	queueFamilies.resize(std::distance(queueFamilies.begin(), std::unique(queueFamilies.begin(), queueFamilies.end())));

	VmaAllocationCreateInfo layerAllocCreateInfo{};
	layerAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkBufferCreateInfo terrainLayerInfo{};
	terrainLayerInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	terrainLayerInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	terrainLayerInfo.size = 100 * sizeof(TerrainLayerProfile) + sizeof(TerrainLayerProfile);
	terrainLayerInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	m_TerrainLayer = std::make_unique<Buffer>(m_Scope, terrainLayerInfo, layerAllocCreateInfo);

	int Layers = 1;
	float Scale = 1e1;
	TerrainLayerProfile defaultTerrain{};
	m_TerrainLayer->Update(&Layers, sizeof(int));
	m_TerrainLayer->Update(&Layers, Scale, sizeof(int));
	m_TerrainLayer->Update(&defaultTerrain, 100 * sizeof(TerrainLayerProfile), 16u);

	TerrainVBs.resize(m_ResourceCount);

	VkCommandBuffer cmd;
	m_Scope.GetQueue(VK_QUEUE_TRANSFER_BIT)
		.AllocateCommandBuffers(1, &cmd);

	::BeginOneTimeSubmitCmd(cmd);

	for (uint32_t i = 0; i < TerrainVBs.size(); i++)
	{
		VkBufferCreateInfo sbInfo{};
		sbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		sbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		sbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		sbInfo.queueFamilyIndexCount = queueFamilies.size();
		sbInfo.pQueueFamilyIndices = queueFamilies.data();
		sbInfo.size = VB.GetDescriptor().range;

		VmaAllocationCreateInfo sbAlloc{};
		sbAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		TerrainVBs[i] = std::make_unique<Buffer>(m_Scope, sbInfo, sbAlloc);

		VkBufferCopy region{};
		region.size = VB.GetDescriptor().range;
		vkCmdCopyBuffer(cmd, VB.GetBuffer(), TerrainVBs[i]->GetBuffer(), 1, &region);
	}

	::EndCommandBuffer(cmd);

	m_Scope.GetQueue(VK_QUEUE_TRANSFER_BIT)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	const uint32_t VertexCount = VB.GetDescriptor().range / sizeof(TerrainVertex);

	const uint32_t m = (glm::max(shape.m_VerPerRing, 7u) + 1) / 4;
	const uint32_t LUTExtent = static_cast<uint32_t>(2 * (m + 2) + 1);

	m_TerrainDispatches = VertexCount / 32 + static_cast<uint32_t>(VertexCount % 32 > 0);

	VkImageCreateInfo noiseInfo{};
	noiseInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	noiseInfo.arrayLayers = shape.m_Rings;
	noiseInfo.extent = { LUTExtent, LUTExtent, 1 };
	noiseInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	noiseInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	noiseInfo.mipLevels = 1;
	noiseInfo.flags = 0u;
	noiseInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	noiseInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	noiseInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	noiseInfo.queueFamilyIndexCount = queueFamilies.size();
	noiseInfo.pQueueFamilyIndices = queueFamilies.data();
	noiseInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	noiseInfo.imageType = VK_IMAGE_TYPE_2D;

	VkImageCreateInfo waterInfo{};
	waterInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	waterInfo.arrayLayers = shape.m_Rings;
	waterInfo.extent = { LUTExtent, LUTExtent, 1 };
	waterInfo.format = VK_FORMAT_R32_SFLOAT;
	waterInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	waterInfo.mipLevels = 1;
	waterInfo.flags = 0u;
	waterInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	waterInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	waterInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	waterInfo.queueFamilyIndexCount = queueFamilies.size();
	waterInfo.pQueueFamilyIndices = queueFamilies.data();
	waterInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	waterInfo.imageType = VK_IMAGE_TYPE_2D;

	VmaAllocationCreateInfo noiseAllocCreateInfo{};
	noiseAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	m_GrassSet.resize(m_ResourceCount);
	m_TerrainLUT.resize(m_ResourceCount);
	m_TerrainSet.resize(m_ResourceCount);
	m_GrassIndirect.resize(m_ResourceCount);
	m_GrassPositions.resize(m_ResourceCount);
	m_TerrainDrawSet.resize(m_ResourceCount);

	VkCommandBuffer clearCMD;
	VkClearColorValue Color;
	Color.float32[0] = 0.0;
	Color.float32[1] = 0.0;
	Color.float32[2] = 0.0;
	Color.float32[3] = 0.0;

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers2(1, &clearCMD);

	::BeginOneTimeSubmitCmd(clearCMD);
	for (uint32_t i = 0; i < m_ResourceCount; i++)
	{
		m_TerrainLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, noiseInfo, noiseAllocCreateInfo);
		m_TerrainLUT[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_TerrainLUT[i].Image);

		vkCmdClearColorImage(clearCMD, m_TerrainLUT[i].Image->GetImage(), VK_IMAGE_LAYOUT_GENERAL, &Color, 1, &m_TerrainLUT[i].Image->GetSubResourceRange());

		m_TerrainLUT[i].Image->TransitionLayout(clearCMD, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_GRAPHICS_BIT);
	}
	::EndCommandBuffer(clearCMD);

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.Submit(clearCMD)
		.Wait()
		.FreeCommandBuffers(1, &clearCMD);

	uint32_t firstRing = m_TerrainLUT[0].Image->GetExtent().width * m_TerrainLUT[0].Image->GetExtent().height;
	uint32_t nextRings = firstRing - glm::ceil(float(m_TerrainLUT[0].Image->GetExtent().width) / 2.0) * glm::ceil(float(m_TerrainLUT[0].Image->GetExtent().height) / 2.0);

	VmaAllocationCreateInfo grassAllocCreateInfo{};
	grassAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	VkBufferCreateInfo grassInfo{};
	grassInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	grassInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	grassInfo.size = sizeof(VkDrawIndirectCommand) * (shape.m_Rings + 2);
	grassInfo.queueFamilyIndexCount = queueFamilies.size();
	grassInfo.pQueueFamilyIndices = queueFamilies.data();
	grassInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	if (shape.m_GrassRings > 0)
	{
		m_GrassIndirectRef = std::make_unique<Buffer>(m_Scope, grassInfo, grassAllocCreateInfo);

		VkDispatchIndirectCommand computeCommand = { 0, 1, 1 };
		VkDrawIndirectCommand* commandsDraw = new VkDrawIndirectCommand[shape.m_Rings];

		uint32_t vtx_offset = 0u;
		for (uint32_t i = 0; i < shape.m_Rings; i++)
		{
			commandsDraw[i].vertexCount = i == 0 ? 27 : (i == 1 ? 15 : 3);
			commandsDraw[i].instanceCount = 0;
			commandsDraw[i].firstVertex = i;
			commandsDraw[i].firstInstance = vtx_offset;

			vtx_offset += i == 0 ? firstRing : nextRings;
			computeCommand.x += i < shape.m_GrassRings ? vtx_offset : 0;
		}
		computeCommand.x /= 32u;

		m_GrassIndirectRef->Update(&computeCommand, sizeof(VkDispatchIndirectCommand), 0u);
		m_GrassIndirectRef->Update(commandsDraw, sizeof(VkDrawIndirectCommand) * shape.m_Rings, sizeof(VkDrawIndirectCommand));
		m_GrassIndirectRef->Update((void*)&shape.m_GrassRings, sizeof(uint32_t), m_GrassIndirectRef->GetSize() - sizeof(uint32_t));

		delete[] commandsDraw;
	}

	grassAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	for (uint32_t i = 0; i < m_TerrainLUT.size(); i++)
	{
		if (shape.m_GrassRings > 0)
		{
			grassInfo.size = sizeof(VkDrawIndirectCommand) * (shape.m_Rings + 1);
			grassInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
			m_GrassIndirect[i] = std::make_unique<Buffer>(m_Scope, grassInfo, grassAllocCreateInfo);

			grassInfo.size = sizeof(glm::ivec4) * (firstRing + nextRings * (shape.m_GrassRings - 1));
			grassInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			m_GrassPositions[i] = std::make_unique<Buffer>(m_Scope, grassInfo, grassAllocCreateInfo);

			m_GrassSet[i] = DescriptorSetDescriptor()
				.AddImageSampler(0, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TerrainLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearClamp, 1))
				.AddStorageBuffer(1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, *TerrainVBs[i])
				.AddStorageBuffer(2, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, *m_GrassPositions[i])
				.AddStorageBuffer(3, VK_SHADER_STAGE_COMPUTE_BIT, *m_GrassIndirect[i])
				.Allocate(m_Scope);
		}


		m_TerrainSet[i] = DescriptorSetDescriptor()
			.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_TerrainLUT[i].View->GetImageView())
			.AddStorageBuffer(1, VK_SHADER_STAGE_COMPUTE_BIT, *m_TerrainLayer)
			.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_TerrainLUT[(i == 0 ? m_TerrainLUT.size() : i) - 1].View->GetImageView(), m_Scope.GetSampler(ESamplerType::PointClamp, 1))
			.AddStorageBuffer(3, VK_SHADER_STAGE_COMPUTE_BIT, VB)
			.AddStorageBuffer(4, VK_SHADER_STAGE_COMPUTE_BIT, *TerrainVBs[i])
			.AddStorageBuffer(5, VK_SHADER_STAGE_COMPUTE_BIT, *TerrainVBs[(i == 0 ? m_TerrainLUT.size() : i) - 1])
			.Allocate(m_Scope);

		m_TerrainDrawSet[i] = DescriptorSetDescriptor()
			.AddImageSampler(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_TerrainLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearClamp, 1))
			// .AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, m_WaterLUT[i].View->GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearClamp, 1))
			.Allocate(m_Scope);
	}

	{
		m_TerrainCompute = ComputePipelineDescriptor()
			.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
			.AddDescriptorLayout(m_TerrainSet[0]->GetLayout())
			.AddSpecializationConstant(0, Rg)
			.AddSpecializationConstant(1, Rt)
			.AddSpecializationConstant(2, VertexCount)
			.AddSpecializationConstant(3, shape.m_Scale)
			.AddSpecializationConstant(4, shape.m_MinHeight)
			.AddSpecializationConstant(5, glm::max(shape.m_MaxHeight, shape.m_MinHeight + 1))
			.AddSpecializationConstant(6, shape.m_NoiseSeed)
			.AddSpecializationConstant(7, float(glm::ceil(float(shape.m_Rings) / 3.f) + 1.f))
			.AddSpecializationConstant(8, shape.m_Rings)
			.SetShaderName("terrain_noise_comp")
			.Construct(m_Scope);

		VkPushConstantRange ConstantCompose{};
		ConstantCompose.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		ConstantCompose.size = sizeof(int);

		m_TerrainCompose = ComputePipelineDescriptor()
			.AddDescriptorLayout(m_TerrainSet[0]->GetLayout())
			.AddSpecializationConstant(0, Rg)
			.AddSpecializationConstant(1, Rt)
			.AddSpecializationConstant(2, VertexCount)
			.AddSpecializationConstant(3, shape.m_Scale)
			.AddSpecializationConstant(4, shape.m_MinHeight)
			.AddSpecializationConstant(5, glm::max(shape.m_MaxHeight, shape.m_MinHeight + 1))
			.AddSpecializationConstant(6, shape.m_NoiseSeed)
			.AddPushConstant(ConstantCompose)
			.SetShaderName("terrain_compose_comp")
			.Construct(m_Scope);

		std::unique_ptr<DescriptorSet> dummy = create_terrain_set(*m_DefaultWhite->Views[1], *m_DefaultNormal->Views[1], *m_DefaultARM->Views[1]);

		if (shape.m_GrassRings > 0)
		{
			m_GrassPipeline = GraphicsPipelineDescriptor()
				.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
				.AddDescriptorLayout(m_GrassSet[0]->GetLayout())
				.SetShaderStage("grass_vert", VK_SHADER_STAGE_VERTEX_BIT)
				.SetShaderStage("grass_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
				.SetCullMode(VK_CULL_MODE_NONE)
				.SetRenderPass(m_Scope.GetTerrainPass(), 0)
				.SetBlendAttachments(3, nullptr)
				.AddSpecializationConstant(0, Rg, VK_SHADER_STAGE_VERTEX_BIT)
				.AddSpecializationConstant(1, Rt, VK_SHADER_STAGE_VERTEX_BIT)
				.AddSpecializationConstant(2, shape.m_Scale, VK_SHADER_STAGE_VERTEX_BIT)
				.AddSpecializationConstant(3, shape.m_MinHeight, VK_SHADER_STAGE_VERTEX_BIT)
				.AddSpecializationConstant(4, glm::max(shape.m_MaxHeight, shape.m_MinHeight + 1), VK_SHADER_STAGE_VERTEX_BIT)
				.AddSpecializationConstant(5, float(glm::ceil(float(shape.m_Rings) / 3.f) + 1.f), VK_SHADER_STAGE_VERTEX_BIT)
				.AddSpecializationConstant(6, shape.m_NoiseSeed, VK_SHADER_STAGE_VERTEX_BIT)
				.Construct(m_Scope);
		}

		m_TerrainTexturingPipeline = GraphicsPipelineDescriptor()
			.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
			.AddDescriptorLayout(dummy->GetLayout())
			.AddDescriptorLayout(m_SubpassDescriptors[0]->GetLayout())
			.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
			.SetShaderStage("terrain_apply_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
			.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, 0u, static_cast<uint32_t>(PBRConstants::FragmentSize()) })
			.SetCullMode(VK_CULL_MODE_NONE)
			.SetRenderPass(m_Scope.GetTerrainPass(), 1)
			.SetBlendAttachments(3, nullptr)
			.Construct(m_Scope);

		m_GrassOcclude = ComputePipelineDescriptor()
			.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
			.AddDescriptorLayout(m_GrassSet[0]->GetLayout())
			.AddSpecializationConstant(0, Rg)
			.AddSpecializationConstant(1, Rt)
			.AddSpecializationConstant(2, shape.m_GrassRings)
			.SetShaderName("grass_indirect_comp")
			.Construct(m_Scope);
	}

	// we can refine clouds discard range
	{
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
			.AddSpecializationConstant(0, float(Rg + shape.m_MinHeight))
			.AddSpecializationConstant(1, Rt);

		m_VolumetricsAbovePipeline = VolumetricPSO.SetShaderName("volumetric_above_comp")
			.Construct(m_Scope);

		m_VolumetricsBetweenPipeline = VolumetricPSO.SetShaderName("volumetric_between_comp")
			.Construct(m_Scope);

		m_VolumetricsUnderPipeline = VolumetricPSO.SetShaderName("volumetric_under_comp")
			.Construct(m_Scope);

		vkDestroyDescriptorSetLayout(m_Scope.GetDevice(), dummy, VK_NULL_HANDLE);
	}

	return 1;
}