#include "pch.hpp"
#include "pipeline.hpp"

GraphicsPipeline::~GraphicsPipeline()
{
	vkDestroyPipelineLayout(scope.GetDevice(), pipelineLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(scope.GetDevice(), pipeline, VK_NULL_HANDLE);
}

GraphicsPipeline& GraphicsPipeline::BindPipeline(VkCommandBuffer cmd)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	return *this;
}

GraphicsPipeline& GraphicsPipeline::PushConstants(VkCommandBuffer cmd, const void* data, size_t dataSize, size_t offset, VkShaderStageFlagBits stages)
{
	vkCmdPushConstants(cmd, pipelineLayout, stages, offset, dataSize, data);

	return *this;
}

ComputePipeline::~ComputePipeline()
{
	vkDestroyPipelineLayout(scope.GetDevice(), pipelineLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(scope.GetDevice(), pipeline, VK_NULL_HANDLE);
}

ComputePipeline& ComputePipeline::BindPipeline(VkCommandBuffer cmd)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	return *this;
}

ComputePipeline& ComputePipeline::PushConstants(VkCommandBuffer cmd, const void* data, size_t dataSize, size_t offset, VkShaderStageFlagBits stages)
{
	vkCmdPushConstants(cmd, pipelineLayout, stages, offset, dataSize, data);

	return *this;
}

PipelineDescriptor::PipelineDescriptor()
{

}

ComputePipelineDescriptor::ComputePipelineDescriptor()
{

}

ComputePipelineDescriptor::~ComputePipelineDescriptor()
{

}

ComputePipelineDescriptor& ComputePipelineDescriptor::SetShaderName(const std::string& inShaderName)
{
	shaderNames[VK_SHADER_STAGE_COMPUTE_BIT] = inShaderName;
	
	return *this;
}

ComputePipelineDescriptor& ComputePipelineDescriptor::AddDescriptorLayout(VkDescriptorSetLayout layout)
{
	descriptorLayouts.push_back(layout);
	
	return *this;
}

ComputePipelineDescriptor& ComputePipelineDescriptor::AddPushConstant(VkPushConstantRange constantRange)
{
	pushConstants.push_back(constantRange);

	return *this;
}

ComputePipelineDescriptor& ComputePipelineDescriptor::AddSpecializationConstant(uint32_t id, std::any value)
{
	specializationConstants[VK_SHADER_STAGE_COMPUTE_BIT][id] = value;

	return *this;
}

std::unique_ptr<ComputePipeline> ComputePipelineDescriptor::Construct(const RenderScope& Scope)
{
	//assert(pipeline == VK_NULL_HANDLE && pipelineLayout == VK_NULL_HANDLE && shaderNames[VK_SHADER_STAGE_COMPUTE_BIT] != "");

	std::unique_ptr<ComputePipeline> out = std::make_unique<ComputePipeline>(Scope);

	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = descriptorLayouts.size();
	pipelineLayoutCI.pSetLayouts = descriptorLayouts.data();
	pipelineLayoutCI.pushConstantRangeCount = pushConstants.size();
	pipelineLayoutCI.pPushConstantRanges = pushConstants.data();
	vkCreatePipelineLayout(Scope.GetDevice(), &pipelineLayoutCI, VK_NULL_HANDLE, &out->pipelineLayout);

	VkShaderModule shader = VK_NULL_HANDLE;

	std::ifstream shaderFile("shaders\\" + shaderNames[VK_SHADER_STAGE_COMPUTE_BIT] + ".spv", std::ios::ate | std::ios::binary);
	std::size_t fileSize = (std::size_t)shaderFile.tellg();
	shaderFile.seekg(0);
	std::vector<char> shaderCode(fileSize);
	shaderFile.read(shaderCode.data(), fileSize);
	shaderFile.close();

	VkShaderModuleCreateInfo shaderModuleCI{};
	shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCI.codeSize = shaderCode.size();
	shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
	vkCreateShaderModule(Scope.GetDevice(), &shaderModuleCI, VK_NULL_HANDLE, &shader);

	std::vector<unsigned char> specialization_data;
	size_t specialization_entry = 0ull, specialization_offset = 0ull;
	std::vector<VkSpecializationMapEntry> specialization_entries(specializationConstants[VK_SHADER_STAGE_COMPUTE_BIT].size());
	for (auto [id, val] : specializationConstants[VK_SHADER_STAGE_COMPUTE_BIT]) {
		std::vector<unsigned char> specialization_bytes(std::move(AnyTypeToBytes(val)));

		specialization_data.resize(specialization_data.size() + specialization_bytes.size());
		specialization_entries[specialization_entry].constantID = id;
		specialization_entries[specialization_entry].offset = specialization_offset;
		specialization_entries[specialization_entry].size = specialization_bytes.size();
		memcpy(&specialization_data[specialization_offset], specialization_bytes.data(), specialization_bytes.size());

		specialization_entry++;
		specialization_offset += specialization_bytes.size();
	}

	VkSpecializationInfo specializationInfo;
	specializationInfo.mapEntryCount = specialization_entries.size();
	specializationInfo.pMapEntries = specialization_entries.data();
	specializationInfo.dataSize = specialization_data.size();
	specializationInfo.pData = specialization_data.data();

	VkPipelineShaderStageCreateInfo pipelineStageCI = {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		VK_SHADER_STAGE_COMPUTE_BIT,
		shader,
		"main",
		&specializationInfo
	};

	VkComputePipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineCI.layout = out->pipelineLayout;
	pipelineCI.stage = pipelineStageCI;
	vkCreateComputePipelines(Scope.GetDevice(), VK_NULL_HANDLE, 1, &pipelineCI, VK_NULL_HANDLE, &out->pipeline);

	vkDestroyShaderModule(Scope.GetDevice(), shader, VK_NULL_HANDLE);

	return out;
}

GraphicsPipelineDescriptor::GraphicsPipelineDescriptor()
{
}

GraphicsPipelineDescriptor::~GraphicsPipelineDescriptor()
{

}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetVertexInputBindings(uint32_t count, VkVertexInputBindingDescription* bindings)
{
	vertexInput.vertexBindingDescriptionCount = count;
	vertexInput.pVertexBindingDescriptions = bindings;

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetVertexAttributeBindings(uint32_t count, VkVertexInputAttributeDescription* attributes)
{
	vertexInput.vertexAttributeDescriptionCount = count;
	vertexInput.pVertexAttributeDescriptions = attributes;

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetPrimitiveTopology(VkPrimitiveTopology topology)
{
	inputAssembly.topology = topology;

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetPolygonMode(VkPolygonMode mode)
{
	rasterizationState.polygonMode = mode;

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetCullMode(VkCullModeFlags mode, VkFrontFace front)
{
	rasterizationState.cullMode = mode;
	rasterizationState.frontFace = front;

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
	rasterizationState.depthBiasEnable = depthBiasConstantFactor != 0.f || depthBiasClamp != 0.f || depthBiasSlopeFactor != 0.f;
	rasterizationState.depthBiasConstantFactor = depthBiasConstantFactor;
	rasterizationState.depthBiasClamp = depthBiasClamp;
	rasterizationState.depthBiasSlopeFactor = depthBiasSlopeFactor;

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetBlendAttachments(uint32_t count, VkPipelineColorBlendAttachmentState* attachments)
{
	blendAttachments.resize(count);
	memcpy(blendAttachments.data(), attachments, count * sizeof(VkPipelineColorBlendAttachmentState));
	blendState.pAttachments = blendAttachments.data();
	blendState.attachmentCount = blendAttachments.size();

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetDepthState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp)
{
	depthStencilState.depthTestEnable = depthTestEnable;
	depthStencilState.depthWriteEnable = depthWriteEnable;
	depthStencilState.depthCompareOp = depthCompareOp;

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetSampling(VkSampleCountFlagBits samples)
{
	multisampleState.rasterizationSamples = samples;

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetShaderStage(const std::string& inShaderName, VkShaderStageFlagBits inStage)
{
	shaderNames[inStage] = inShaderName;

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::AddDescriptorLayout(VkDescriptorSetLayout layout)
{
	descriptorLayouts.push_back(layout);

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::AddPushConstant(VkPushConstantRange constantRange)
{
	pushConstants.push_back(constantRange);

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::AddSpecializationConstant(uint32_t id, std::any value, VkShaderStageFlagBits stage)
{
	specializationConstants[stage][id] = value;

	return *this;
}

GraphicsPipelineDescriptor& GraphicsPipelineDescriptor::SetRenderPass(const VkRenderPass& RenderPass, uint32_t Subpass)
{
	renderPass = RenderPass;
	subpass = Subpass;

	return *this;
}

//TODO: Check pipeline cache?
std::unique_ptr<GraphicsPipeline> GraphicsPipelineDescriptor::Construct(const RenderScope& Scope)
{
	//assert(pipeline == VK_NULL_HANDLE && pipelineLayout == VK_NULL_HANDLE && shaderNames.size() > 0);

	std::unique_ptr<GraphicsPipeline> out = std::make_unique<GraphicsPipeline>(Scope);

	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = descriptorLayouts.size();
	pipelineLayoutCI.pSetLayouts = descriptorLayouts.data();
	pipelineLayoutCI.pushConstantRangeCount = pushConstants.size();
	pipelineLayoutCI.pPushConstantRanges = pushConstants.data();
	vkCreatePipelineLayout(Scope.GetDevice(), &pipelineLayoutCI, VK_NULL_HANDLE, &out->pipelineLayout);

	std::vector<VkShaderModule> shaders;
	std::vector<VkSpecializationInfo> specInfos;
	std::vector<VkSpecializationMapEntry> specialization_entries;
	std::vector<unsigned char> specialization_data;

	std::vector<VkPipelineShaderStageCreateInfo> pipelineStagesCI{};
	const std::array<const VkShaderStageFlagBits, 5> stages = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };

	for (size_t i = 0; i < stages.size(); i++)
	{
		if (shaderNames.count(stages[i]) > 0)
		{
			std::ifstream shaderFile("shaders\\" + shaderNames[stages[i]] + ".spv", std::ios::ate | std::ios::binary);
			std::size_t fileSize = (std::size_t)shaderFile.tellg();
			shaderFile.seekg(0);
			std::vector<char> shaderCode(fileSize);
			shaderFile.read(shaderCode.data(), fileSize);
			shaderFile.close();

			VkShaderModuleCreateInfo shaderModuleCI{};
			shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderModuleCI.codeSize = shaderCode.size();
			shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

			VkShaderModule shaderModule;
			vkCreateShaderModule(Scope.GetDevice(), &shaderModuleCI, VK_NULL_HANDLE, &shaderModule);

			shaders.push_back(shaderModule);

			size_t specialization_entry = 0ull, specialization_offset = 0ull, data_offset = specialization_data.size(), entry_offset = specialization_entries.size();
			specialization_entries.resize(specialization_entries.size() + specializationConstants[stages[i]].size());

			for (auto [id, val] : specializationConstants[stages[i]]) {
				std::vector<unsigned char> specialization_bytes(std::move(AnyTypeToBytes(val)));

				specialization_data.resize(specialization_data.size() + specialization_bytes.size());
				specialization_entries[specialization_entry].constantID = id;
				specialization_entries[specialization_entry].offset = specialization_offset;
				specialization_entries[specialization_entry].size = specialization_bytes.size();
				memcpy(&specialization_data[specialization_offset], specialization_bytes.data(), specialization_bytes.size());

				specialization_offset += specialization_bytes.size();
				specialization_entry++;
			}

			if (specializationConstants[stages[i]].size() > 0)
			{
				VkSpecializationInfo specializationInfo;
				specializationInfo.mapEntryCount = specialization_entries.size();
				specializationInfo.pMapEntries = specialization_entries.data() + entry_offset;
				specializationInfo.dataSize = specialization_data.size();
				specializationInfo.pData = specialization_data.data() + data_offset;

				specInfos.push_back(specializationInfo);

				pipelineStagesCI.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, VK_NULL_HANDLE, 0, stages[i], shaderModule, "main", &specInfos[specInfos.size() - 1]);
			}
			else
			{
				pipelineStagesCI.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, VK_NULL_HANDLE, 0, stages[i], shaderModule, "main", nullptr);
			}
		}
	}

	VkGraphicsPipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.subpass = subpass;
	pipelineCI.renderPass = renderPass ? renderPass : Scope.GetRenderPass();
	pipelineCI.pInputAssemblyState = &inputAssembly;
	pipelineCI.pVertexInputState = &vertexInput;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pColorBlendState = &blendState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.stageCount = pipelineStagesCI.size();
	pipelineCI.pStages = pipelineStagesCI.data();
	pipelineCI.layout = out->pipelineLayout;
	vkCreateGraphicsPipelines(Scope.GetDevice(), VK_NULL_HANDLE, 1, &pipelineCI, VK_NULL_HANDLE, &out->pipeline);

	for (auto& shader : shaders) {
		vkDestroyShaderModule(Scope.GetDevice(), shader, VK_NULL_HANDLE);
	}

	return out;
}