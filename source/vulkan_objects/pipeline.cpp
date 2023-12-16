#include "pch.hpp"
#include "pipeline.hpp"

extern std::string exec_path;

Pipeline::Pipeline(const RenderScope& InScope)
	: Scope(&InScope)
{

}

ComputePipeline::ComputePipeline(const RenderScope& Scope)
	: Pipeline(Scope)
{

}

ComputePipeline::~ComputePipeline()
{

}

ComputePipeline& ComputePipeline::SetShaderName(const std::string& inIhaderName)
{
	shaderName = inIhaderName;
	
	return *this;
}

ComputePipeline& ComputePipeline::AddDescriptorLayout(VkDescriptorSetLayout layout)
{
	descriptorLayouts.push_back(layout);
	
	return *this;
}

void ComputePipeline::Construct()
{
	assert(pipeline == VK_NULL_HANDLE && pipelineLayout == VK_NULL_HANDLE && shaderName != "");

	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = descriptorLayouts.size();
	pipelineLayoutCI.pSetLayouts = descriptorLayouts.data();
	pipelineLayoutCI.pushConstantRangeCount = pushConstants.size();
	pipelineLayoutCI.pPushConstantRanges = pushConstants.data();
	vkCreatePipelineLayout(Scope->GetDevice(), &pipelineLayoutCI, VK_NULL_HANDLE, &pipelineLayout);

	VkShaderModule shader = VK_NULL_HANDLE;
	VkPipelineShaderStageCreateInfo pipelineStageCI{};

	std::ifstream shaderFile(exec_path + "shaders\\" + shaderName + "_comp.spv", std::ios::ate | std::ios::binary);
	std::size_t fileSize = (std::size_t)shaderFile.tellg();
	shaderFile.seekg(0);
	std::vector<char> shaderCode(fileSize);
	shaderFile.read(shaderCode.data(), fileSize);
	shaderFile.close();

	VkShaderModuleCreateInfo shaderModuleCI{};
	shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCI.codeSize = shaderCode.size();
	shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
	vkCreateShaderModule(Scope->GetDevice(), &shaderModuleCI, VK_NULL_HANDLE, &shader);

	pipelineStageCI = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, VK_NULL_HANDLE, 0, VK_SHADER_STAGE_COMPUTE_BIT, shader, "main", VK_NULL_HANDLE };

	VkPipelineShaderStageCreateInfo compShaderStageInfo{};
	compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = shader;
	compShaderStageInfo.pName = "main";

	VkComputePipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineCI.layout = pipelineLayout;
	pipelineCI.stage = pipelineStageCI;
	vkCreateComputePipelines(Scope->GetDevice(), VK_NULL_HANDLE, 1, &pipelineCI, VK_NULL_HANDLE, &pipeline);

	vkDestroyShaderModule(Scope->GetDevice(), shader, VK_NULL_HANDLE);
}

void ComputePipeline::BindPipeline(VkCommandBuffer cmd) const
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

Pipeline::~Pipeline()
{
	vkDestroyPipelineLayout(Scope->GetDevice(), pipelineLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(Scope->GetDevice(), pipeline, VK_NULL_HANDLE);
}

GraphicsPipeline::GraphicsPipeline(const RenderScope& InScope)
	: Pipeline(InScope)
{
}

GraphicsPipeline::~GraphicsPipeline()
{

}

GraphicsPipeline& GraphicsPipeline::SetVertexInputBindings(uint32_t count, VkVertexInputBindingDescription* bindings)
{
	vertexInput.vertexBindingDescriptionCount = count;
	vertexInput.pVertexBindingDescriptions = bindings;

	return *this;
}

GraphicsPipeline& GraphicsPipeline::SetVertexAttributeBindings(uint32_t count, VkVertexInputAttributeDescription* attributes)
{
	vertexInput.vertexAttributeDescriptionCount = count;
	vertexInput.pVertexAttributeDescriptions = attributes;

	return *this;
}

GraphicsPipeline& GraphicsPipeline::SetPrimitiveTopology(VkPrimitiveTopology topology)
{
	inputAssembly.topology = topology;

	return *this;
}

GraphicsPipeline& GraphicsPipeline::SetPolygonMode(VkPolygonMode mode)
{
	rasterizationState.polygonMode = mode;

	return *this;
}

GraphicsPipeline& GraphicsPipeline::SetCullMode(VkCullModeFlags mode, VkFrontFace front)
{
	rasterizationState.cullMode = mode;
	rasterizationState.frontFace = front;

	return *this;
}

GraphicsPipeline& GraphicsPipeline::SetDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
	rasterizationState.depthBiasEnable = depthBiasConstantFactor != 0.f && depthBiasClamp != 0.f && depthBiasSlopeFactor != 0.f;
	rasterizationState.depthBiasConstantFactor = depthBiasConstantFactor;
	rasterizationState.depthBiasClamp = depthBiasClamp;
	rasterizationState.depthBiasSlopeFactor = depthBiasSlopeFactor;

	return *this;
}

GraphicsPipeline& GraphicsPipeline::SetBlendAttachments(uint32_t count, VkPipelineColorBlendAttachmentState* attachments)
{
	blendAttachments.resize(count);
	memcpy(blendAttachments.data(), attachments, count * sizeof(VkPipelineColorBlendAttachmentState));
	blendState.pAttachments = blendAttachments.data();
	blendState.attachmentCount = blendAttachments.size();

	return *this;
}

GraphicsPipeline& GraphicsPipeline::SetDepthState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp)
{
	depthStencilState.depthTestEnable = depthTestEnable;
	depthStencilState.depthWriteEnable = depthWriteEnable;
	depthStencilState.depthCompareOp = depthCompareOp;

	return *this;
}

GraphicsPipeline& GraphicsPipeline::SetSampling(VkSampleCountFlagBits samples)
{
	multisampleState.rasterizationSamples = samples;

	return *this;
}

GraphicsPipeline& GraphicsPipeline::SetShaderStages(const std::string& inShaderName, VkShaderStageFlagBits inStages)
{
	shaderName = inShaderName;
	shaderStagesFlags = inStages;

	return *this;
}

GraphicsPipeline& GraphicsPipeline::AddDescriptorLayout(VkDescriptorSetLayout layout)
{
	descriptorLayouts.push_back(layout);

	return *this;
}

GraphicsPipeline& GraphicsPipeline::SetSubpass(uint32_t inSubpass)
{
	subpass = inSubpass;

	return *this;
}

//TODO: Check pipeline cache?
void GraphicsPipeline::Construct()
{
	assert(pipeline == VK_NULL_HANDLE && pipelineLayout == VK_NULL_HANDLE && shaderName != "");

	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = descriptorLayouts.size();
	pipelineLayoutCI.pSetLayouts = descriptorLayouts.data();
	pipelineLayoutCI.pushConstantRangeCount = pushConstants.size();
	pipelineLayoutCI.pPushConstantRanges = pushConstants.data();
	vkCreatePipelineLayout(Scope->GetDevice(), &pipelineLayoutCI, VK_NULL_HANDLE, &pipelineLayout);

	std::vector<VkShaderModule> shaders;
	std::vector<VkPipelineShaderStageCreateInfo> pipelineStagesCI{};
	const std::array<const VkShaderStageFlagBits, 5> stages = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
	const std::array<const char*, 5> stagePostfixes = { "_vert.spv", "_tesc.spv", "_tese.spv", "_geom.spv", "_frag.spv" };

	for (size_t i = 0; i < stages.size(); i++)
	{
		if ((shaderStagesFlags & stages[i]) != 0)
		{
			std::ifstream shaderFile(exec_path + "shaders\\" + shaderName + stagePostfixes[i], std::ios::ate | std::ios::binary);
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
			vkCreateShaderModule(Scope->GetDevice(), &shaderModuleCI, VK_NULL_HANDLE, &shaderModule);

			shaders.push_back(shaderModule);
			pipelineStagesCI.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, VK_NULL_HANDLE, 0, stages[i], shaderModule, "main", VK_NULL_HANDLE);
		}
	}

	VkGraphicsPipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.subpass = subpass;
	pipelineCI.renderPass = Scope->GetRenderPass();
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
	pipelineCI.layout = pipelineLayout;
	vkCreateGraphicsPipelines(Scope->GetDevice(), VK_NULL_HANDLE, 1, &pipelineCI, VK_NULL_HANDLE, &pipeline);

	for (auto& shader : shaders) {
		vkDestroyShaderModule(Scope->GetDevice(), shader, VK_NULL_HANDLE);
	}
}

void GraphicsPipeline::BindPipeline(VkCommandBuffer cmd) const
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}