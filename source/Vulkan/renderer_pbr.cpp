#include "pch.hpp"
#include "renderer.hpp"

#define WRAPL(i) (i == 0 ? m_ResourceCount : i) - 1
#define WRAPR(i) i == m_ResourceCount - 1 ? 0 : i + 1

void VulkanBase::_drawObject(const PBRObject& gro, const PBRConstants& constants) const
{
	if (m_Scope.GetSwapchainExtent().width == 0 || m_Scope.GetSwapchainExtent().height == 0)
		return;

	assert(m_InFrame, "Call BeginFrame first!");

	const VkCommandBuffer& cmd = m_DeferredSync[m_ResourceIndex].Commands;
	const VkDeviceSize offsets[] = { 0 };

	m_UBOSets[m_ResourceIndex]->BindSet(0, cmd, *gro.pipeline);
	gro.descriptorSet->BindSet(1, cmd, *gro.pipeline);
	gro.pipeline->PushConstants(cmd, &constants.Offset, PBRConstants::VertexSize(), 0u, VK_SHADER_STAGE_VERTEX_BIT);
	gro.pipeline->PushConstants(cmd, &constants.Color, PBRConstants::FragmentSize(), PBRConstants::VertexSize(), VK_SHADER_STAGE_FRAGMENT_BIT);
	gro.pipeline->BindPipeline(cmd);

	vkCmdBindVertexBuffers(cmd, 0, 1, &gro.mesh->GetVertexBuffer()->GetBuffer(), offsets);
	vkCmdBindIndexBuffer(cmd, gro.mesh->GetIndexBuffer()->GetBuffer(), 0, gro.mesh->GetIndexType());
	vkCmdDrawIndexed(cmd, gro.mesh->GetIndicesCount(), 1, 0, 0, 0);
}

void VulkanBase::_updateObject(entt::entity ent, entt::registry& registry) const
{
	VulkanTexture* albedo = static_cast<VulkanTexture*>(registry.get<GR::Components::AlbedoMap>(ent).Get().get());
	VulkanTexture* nh = static_cast<VulkanTexture*>(registry.get<GR::Components::NormalDisplacementMap>(ent).Get().get());
	VulkanTexture* arm = static_cast<VulkanTexture*>(registry.get<GR::Components::AORoughnessMetallicMapTransmittance>(ent).Get().get());

	PBRObject& gro = registry.get<PBRObject>(ent);
	gro.descriptorSet = create_pbr_set(*albedo->View, *nh->View, *arm->View);
	gro.dirty = false;
}

std::unique_ptr<DescriptorSet> VulkanBase::create_pbr_set(const VulkanImageView& albedo
	, const VulkanImageView& nh
	, const VulkanImageView& arm) const
{
	return DescriptorSetDescriptor()
		.AddImageSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT, albedo.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, albedo.GetSubresourceRange().levelCount))
		.AddImageSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT, nh.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, nh.GetSubresourceRange().levelCount))
		.AddImageSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT, arm.GetImageView(), m_Scope.GetSampler(ESamplerType::BillinearRepeat, arm.GetSubresourceRange().levelCount))
		.Allocate(m_Scope);
}

std::unique_ptr<GraphicsPipeline> VulkanBase::create_pbr_pipeline(const DescriptorSet& set) const
{
	auto vertAttributes = MeshVertex::getAttributeDescriptions();
	auto vertBindings = MeshVertex::getBindingDescription();

	return GraphicsPipelineDescriptor()
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.SetBlendAttachments(3, nullptr)
		.SetVertexInputBindings(1, &vertBindings)
		.SetVertexAttributeBindings(vertAttributes.size(), vertAttributes.data())
		.SetShaderStage("default_vert", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("default_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddDescriptorLayout(m_UBOSets[0]->GetLayout())
		.AddDescriptorLayout(set.GetLayout())
		.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<uint32_t>(PBRConstants::VertexSize()) })
		.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(PBRConstants::VertexSize()), static_cast<uint32_t>(PBRConstants::FragmentSize()) })
		.AddSpecializationConstant(0, Rg, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddSpecializationConstant(1, Rt, VK_SHADER_STAGE_FRAGMENT_BIT)
		.Construct(m_Scope);
}

VkBool32 VulkanBase::brdf_precompute()
{
	std::vector<uint32_t> queueFamilies = { m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_COMPUTE_BIT).GetFamilyIndex(), m_Scope.GetQueue(VK_QUEUE_TRANSFER_BIT).GetFamilyIndex() };
	std::sort(queueFamilies.begin(), queueFamilies.end());
	queueFamilies.resize(std::distance(queueFamilies.begin(), std::unique(queueFamilies.begin(), queueFamilies.end())));

	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	m_DiffuseIrradience.resize(m_ResourceCount);
	m_SpecularLUT.resize(m_ResourceCount);
	m_CubemapLUT.resize(m_ResourceCount);

	for (size_t i = 0; i < m_ResourceCount; i++)
	{
		VkImageCreateInfo hdrInfo{};
		hdrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		hdrInfo.format = m_Scope.GetHDRFormat();
		hdrInfo.arrayLayers = 6;
		hdrInfo.extent = { CubeR, CubeR, 1 };
		hdrInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		hdrInfo.imageType = VK_IMAGE_TYPE_2D;
		hdrInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		hdrInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		hdrInfo.mipLevels = 1;
		hdrInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		hdrInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		hdrInfo.queueFamilyIndexCount = queueFamilies.size();
		hdrInfo.pQueueFamilyIndices = queueFamilies.data();
		hdrInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		m_DiffuseIrradience[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_DiffuseIrradience[i].View = std::make_unique<VulkanImageView>(m_Scope, *m_DiffuseIrradience[i].Image);

		hdrInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		hdrInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(CubeR))) + 1;
		m_SpecularLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_SpecularLUT[i].Views.emplace_back(std::make_unique<VulkanImageView>(m_Scope, *m_SpecularLUT[i].Image));

		hdrInfo.arrayLayers = 12;
		m_CubemapLUT[i].Image = std::make_unique<VulkanImage>(m_Scope, hdrInfo, allocCreateInfo);
		m_CubemapLUT[i].Views.emplace_back(std::make_unique<VulkanImageView>(m_Scope, *m_CubemapLUT[i].Image));

		m_SpecularLUT[i].Views.reserve(hdrInfo.mipLevels + 1);
		m_CubemapLUT[i].Views.reserve(hdrInfo.mipLevels + 1);
		VkImageSubresourceRange subRes = m_SpecularLUT[i].Image->GetSubResourceRange();
		for (uint32_t j = 0; j < hdrInfo.mipLevels; j++)
		{
			subRes.baseMipLevel = j;
			subRes.levelCount = 1u;
			m_SpecularLUT[i].Views.emplace_back(std::make_unique<VulkanImageView>(m_Scope, *m_SpecularLUT[i].Image, subRes));
			m_CubemapLUT[i].Views.emplace_back(std::make_unique<VulkanImageView>(m_Scope, *m_CubemapLUT[i].Image, subRes));
		}
	}

	std::unique_ptr<GraphicsPipeline> m_IntegrationPipeline = GraphicsPipelineDescriptor()
		.SetShaderStage("fullscreen", VK_SHADER_STAGE_VERTEX_BIT)
		.SetShaderStage("brdf_integrate_frag", VK_SHADER_STAGE_FRAGMENT_BIT)
		.SetRenderPass(m_Scope.GetSimplePass(), 0)
		.Construct(m_Scope);

	VkImageCreateInfo bimageInfo{};
	bimageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	bimageInfo.format = m_Scope.GetHDRFormat();
	bimageInfo.arrayLayers = 1;
	bimageInfo.extent = { 512, 512, 1 };
	bimageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	bimageInfo.imageType = VK_IMAGE_TYPE_2D;
	bimageInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	bimageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	bimageInfo.mipLevels = 1;
	bimageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	bimageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bimageInfo.queueFamilyIndexCount = queueFamilies.size();
	bimageInfo.pQueueFamilyIndices = queueFamilies.data();

	m_BRDFLUT.Image = std::make_unique<VulkanImage>(m_Scope, bimageInfo, allocCreateInfo);
	m_BRDFLUT.View = std::make_unique<VulkanImageView>(m_Scope, *m_BRDFLUT.Image);

	VkFramebuffer Framebuffer;
	CreateFramebuffer(m_Scope.GetDevice(), m_Scope.GetCubemapPass(), { m_BRDFLUT.Image->GetExtent().width, m_BRDFLUT.Image->GetExtent().height, 1 }, { m_BRDFLUT.View->GetImageView() }, &Framebuffer);

	VkCommandBuffer cmd;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.AllocateCommandBuffers(1, &cmd);
	vkBeginCommandBuffer(cmd, &beginInfo);

	{
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = Framebuffer;
		renderPassInfo.renderPass = m_Scope.GetSimplePass();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { m_BRDFLUT.Image->GetExtent().width, m_BRDFLUT.Image->GetExtent().height };

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(m_BRDFLUT.Image->GetExtent().width);
		viewport.height = static_cast<float>(m_BRDFLUT.Image->GetExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { m_BRDFLUT.Image->GetExtent().width, m_BRDFLUT.Image->GetExtent().height };
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_IntegrationPipeline->BindPipeline(cmd);
		vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdEndRenderPass(cmd);
	}

	vkEndCommandBuffer(cmd);

	m_Scope.GetQueue(VK_QUEUE_GRAPHICS_BIT)
		.Submit(cmd)
		.Wait()
		.FreeCommandBuffers(1, &cmd);

	vkDestroyFramebuffer(m_Scope.GetDevice(), Framebuffer, VK_NULL_HANDLE);

	m_CubemapDescriptors.resize(m_ResourceCount);
	m_DiffuseDescriptors.resize(m_ResourceCount);
	m_ConvolutionDescriptors.resize(m_ResourceCount);
	m_SpecularDescriptors.resize(m_SpecularLUT[0].Image->GetMipLevelsCount() * m_ResourceCount);
	m_CubemapMipDescriptors.resize(m_SpecularLUT[0].Image->GetMipLevelsCount() * m_ResourceCount);

	struct
	{
		int Count = 0;
		float ColorNorm = 0.0;
		std::vector<glm::vec4> data = {};
	} diffuseData;
	diffuseData.data.reserve(663);

	float sampleDelta = 0.25;
	for (float phi = 0.0; phi < 2.0 * glm::pi<float>(); phi += sampleDelta)
	{
		float sinPhi = sin(phi);
		float cosPhi = cos(phi);

		for (float theta = 0.0; theta < 0.5 * glm::pi<float>(); theta += sampleDelta)
		{
			float sinTheta = sin(theta);
			float cosTheta = cos(theta);

			diffuseData.data.emplace_back(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta, sinTheta * cosTheta);
			diffuseData.Count++;
		}
	}
	diffuseData.ColorNorm = glm::pi<float>() / float(diffuseData.Count);

	VmaAllocationCreateInfo bufallocCreateInfo{};
	bufallocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkBufferCreateInfo bufInfo{};
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufInfo.size = sizeof(glm::vec4) * diffuseData.data.size();
	bufInfo.queueFamilyIndexCount = queueFamilies.size();
	bufInfo.pQueueFamilyIndices = queueFamilies.data();
	bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	m_DiffusePrecompute = std::make_unique<Buffer>(m_Scope, bufInfo, bufallocCreateInfo);
	m_DiffusePrecompute->Update(diffuseData.data.data(), sizeof(glm::vec4) * diffuseData.data.size());

	std::vector<glm::vec4> specSamples;
	const int specSamplesCount = 32;
	specSamples.reserve(specSamplesCount);
	for (int i = 0; i < specSamplesCount; i++)
	{
		glm::vec4 sample;
		sample.x = float(i) / float(specSamplesCount);

		// radical inverse
		uint32_t N = static_cast<uint32_t>(specSamplesCount);
		N = (N << 16u) | (N >> 16u);
		N = ((N & 0x55555555u) << 1u) | ((N & 0xAAAAAAAAu) >> 1u);
		N = ((N & 0x33333333u) << 2u) | ((N & 0xCCCCCCCCu) >> 2u);
		N = ((N & 0x0F0F0F0Fu) << 4u) | ((N & 0xF0F0F0F0u) >> 4u);
		N = ((N & 0x00FF00FFu) << 8u) | ((N & 0xFF00FF00u) >> 8u);
		sample.y = float(N) * 2.3283064365386963e-10; // / 0x100000000
		sample.z = 4.0 * glm::pi<float>() / float(6.0 * CubeR * CubeR);

		specSamples.emplace_back(sample);
	}
	bufInfo.size = sizeof(glm::vec4) * specSamples.size();
	m_SpecularPrecompute = std::make_unique<Buffer>(m_Scope, bufInfo, bufallocCreateInfo);
	m_SpecularPrecompute->Update(specSamples.data(), sizeof(glm::vec4) * specSamples.size());

	VkSampler SamplerPoint = m_Scope.GetSampler(ESamplerType::PointClamp, 1);
	VkSampler SamplerLinear = m_Scope.GetSampler(ESamplerType::LinearClamp, 1);
	VkSampler SamplerRepeat = m_Scope.GetSampler(ESamplerType::LinearRepeat, 1);
	for (size_t i = 0; i < m_ResourceCount; i++)
	{
		m_CubemapDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_UBOBuffers[i])
			.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_CubemapLUT[i].Views[0]->GetImageView())
			.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView(), SamplerLinear)
			.Allocate(m_Scope);

		m_ConvolutionDescriptors[i] = DescriptorSetDescriptor()
			.AddUniformBuffer(0, VK_SHADER_STAGE_COMPUTE_BIT, *m_UBOBuffers[i])
			.AddImageSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, m_TransmittanceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(2, VK_SHADER_STAGE_COMPUTE_BIT, m_IrradianceLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(3, VK_SHADER_STAGE_COMPUTE_BIT, m_ScatteringLUT.View->GetImageView(), SamplerLinear)
			.AddImageSampler(4, VK_SHADER_STAGE_COMPUTE_BIT, m_CubemapLUT[i].Views[0]->GetImageView(), m_Scope.GetSampler(ESamplerType::LinearClamp, m_CubemapLUT[i].Views[0]->GetSubresourceRange().levelCount))
			.Allocate(m_Scope);

		m_DiffuseDescriptors[i] = DescriptorSetDescriptor()
			.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_DiffuseIrradience[i].View->GetImageView())
			.AddStorageBuffer(1, VK_SHADER_STAGE_COMPUTE_BIT, *m_DiffusePrecompute)
			.Allocate(m_Scope);

		uint32_t mips = m_SpecularLUT[i].Image->GetMipLevelsCount();
		for (uint32_t j = 0; j < mips; j++)
		{
			m_SpecularDescriptors[i * mips + j] = DescriptorSetDescriptor()
				.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_SpecularLUT[i].Views[j + 1]->GetImageView())
				.AddStorageBuffer(1, VK_SHADER_STAGE_COMPUTE_BIT, *m_SpecularPrecompute)
				.Allocate(m_Scope);

			m_CubemapMipDescriptors[i * mips + j] = DescriptorSetDescriptor()
				.AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, m_CubemapLUT[i].Views[j]->GetImageView())
				.AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT, m_CubemapLUT[i].Views[j + 1]->GetImageView())
				.Allocate(m_Scope);
		}
	}

	m_CubemapPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_CubemapDescriptors[0]->GetLayout())
		.AddSpecializationConstant(0, Rg)
		.AddSpecializationConstant(1, Rt)
		.SetShaderName("cubemap_comp")
		.Construct(m_Scope);

	m_CubemapMipPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_CubemapMipDescriptors[0]->GetLayout())
		.SetShaderName("cubemap_mip_comp")
		.Construct(m_Scope);

	m_ConvolutionPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_ConvolutionDescriptors[0]->GetLayout())
		.AddDescriptorLayout(m_DiffuseDescriptors[0]->GetLayout())
		.SetShaderName("cube_convolution_comp")
		.AddSpecializationConstant(0, diffuseData.Count)
		.AddSpecializationConstant(1, diffuseData.ColorNorm)
		.Construct(m_Scope);

	VkPushConstantRange pushContants{};
	pushContants.size = sizeof(float);
	pushContants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	m_SpecularIBLPipeline = ComputePipelineDescriptor()
		.AddDescriptorLayout(m_ConvolutionDescriptors[0]->GetLayout())
		.AddDescriptorLayout(m_SpecularDescriptors[0]->GetLayout())
		.SetShaderName("cube_convolution_spec_comp")
		.AddPushConstant(pushContants)
		.AddSpecializationConstant(0, Rg)
		.AddSpecializationConstant(1, Rt)
		.AddSpecializationConstant(2, specSamplesCount)
		.Construct(m_Scope);

	return 1;
}