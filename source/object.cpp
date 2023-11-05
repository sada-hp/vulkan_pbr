#include "pch.hpp"
#include "object.hpp"

extern std::string exec_path;

vkShaderPipeline::vkShaderPipeline(const VkDevice& device, const vkShaderPipelineCreateInfo& objectCI)
	: _device(device), _pool(objectCI.descriptorPool)
{
	fill(_device, objectCI);
}

vkShaderPipeline::~vkShaderPipeline()
{
	free();
}

VkBool32 vkShaderPipeline::fill(const VkDevice& device, const vkShaderPipelineCreateInfo& objectCI)
{
	free();

	_device = device;
	_pool = objectCI.descriptorPool;
	VkDescriptorSetLayoutCreateInfo dsetInfo{};
	dsetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsetInfo.bindingCount = objectCI.bindingsCount;
	dsetInfo.pBindings = objectCI.pBindings;
	vkCreateDescriptorSetLayout(device, &dsetInfo, VK_NULL_HANDLE, &descriptorSetLayout);

	VkDescriptorSetAllocateInfo setAlloc{};
	setAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAlloc.descriptorPool = objectCI.descriptorPool;
	setAlloc.descriptorSetCount = 1;
	setAlloc.pSetLayouts = &descriptorSetLayout;
	vkAllocateDescriptorSets(device, &setAlloc, &descriptorSet);

	for (size_t i = 0; i < objectCI.writesCount; i++)
		objectCI.pWrites[i].dstSet = descriptorSet;

	vkUpdateDescriptorSets(device, objectCI.writesCount, objectCI.pWrites, 0, VK_NULL_HANDLE);

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &descriptorSetLayout;
	layoutInfo.pushConstantRangeCount = objectCI.pushConstantsCount;
	layoutInfo.pPushConstantRanges = objectCI.pPushContants;
	vkCreatePipelineLayout(device, &layoutInfo, VK_NULL_HANDLE, &pipelineLayout);

	const std::array<const VkShaderStageFlagBits, 5> _stages = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
	const std::array<const char*, 5> _stage_postfix = { "_vert.spv", "_tesc.spv", "_tese.spv", "_geom.spv", "_frag.spv" };
	std::map<VkShaderStageFlagBits, VkShaderModule> shaders;

	for (size_t i = 0; i < _stages.size(); i++)
	{
		if ((objectCI.shaderStages & _stages[i]) != 0)
		{
			std::ifstream shaderFile(exec_path + "shaders\\" + objectCI.shaderName + _stage_postfix[i], std::ios::ate | std::ios::binary);
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
			vkCreateShaderModule(device, &shaderModuleCI, VK_NULL_HANDLE, &shaderModule);
			shaders[_stages[i]] = shaderModule;
		}
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = objectCI.vertexInputBindingsCount;
	vertexInputInfo.pVertexBindingDescriptions = objectCI.vertexInputBindings;
	vertexInputInfo.vertexAttributeDescriptionCount = objectCI.vertexInputAttributesCount;
	vertexInputInfo.pVertexAttributeDescriptions = objectCI.vertexInputAttributes;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyState.primitiveRestartEnable = VK_FALSE;

	VkPipelineRasterizationStateCreateInfo rasterizationState{};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.lineWidth = 1.0f;
	rasterizationState.cullMode = VK_CULL_MODE_NONE; //VK_CULL_MODE_BACK_BIT;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthBiasEnable = VK_FALSE;
	rasterizationState.depthBiasConstantFactor = 0.0f;
	rasterizationState.depthBiasClamp = 0.0f;
	rasterizationState.depthBiasSlopeFactor = 0.0f;

	VkPipelineColorBlendAttachmentState blendAttachmentState{};
	blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachmentState.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendState{};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &blendAttachmentState;

	VkPipelineDepthStencilStateCreateInfo depthStencilState{};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.pNext = VK_NULL_HANDLE;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.depthBoundsTestEnable = VK_FALSE;
	depthStencilState.minDepthBounds = 0.0f; // Optional
	depthStencilState.maxDepthBounds = 1.0f; // Optional
	depthStencilState.stencilTestEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = VK_NULL_HANDLE;
	viewportState.scissorCount = 1;
	viewportState.pScissors = VK_NULL_HANDLE;

	VkPipelineMultisampleStateCreateInfo multisampleState{};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.sampleShadingEnable = VK_FALSE;
	multisampleState.minSampleShading = 0;

	std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pNext = VK_NULL_HANDLE;
	dynamicState.flags = 0;
	dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicState.pDynamicStates = dynamicStates.data();

	std::vector<VkPipelineShaderStageCreateInfo> pipelineStagesCI;
	pipelineStagesCI.reserve(shaders.size());

	for (const auto& [_stage, _module] : shaders)
		pipelineStagesCI.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, VK_NULL_HANDLE, 0, _stage, _module, "main", VK_NULL_HANDLE);

	VkGraphicsPipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.pVertexInputState = &vertexInputInfo;
	pipelineCI.renderPass = objectCI.renderPass;
	pipelineCI.subpass = objectCI.subpass;
	pipelineCI.layout = pipelineLayout;
	pipelineCI.stageCount = static_cast<uint32_t>(pipelineStagesCI.size());
	pipelineCI.pStages = pipelineStagesCI.data();

	VkBool32 res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, VK_NULL_HANDLE, &pipeline) == VK_SUCCESS;

	for (const auto& [_stage, _module] : shaders)
		vkDestroyShaderModule(device, _module, VK_NULL_HANDLE);

	return res;
}

void vkShaderPipeline::free()
{
	if (pipeline != VK_NULL_HANDLE)
		vkDestroyPipeline(_device, pipeline, VK_NULL_HANDLE);
	if (pipelineLayout != VK_NULL_HANDLE)
		vkDestroyPipelineLayout(_device, pipelineLayout, VK_NULL_HANDLE);
	if (_pool != VK_NULL_HANDLE)
		vkFreeDescriptorSets(_device, _pool, 1, &descriptorSet);
	if (descriptorSetLayout != VK_NULL_HANDLE)
		vkDestroyDescriptorSetLayout(_device, descriptorSetLayout, VK_NULL_HANDLE);

	pipeline = VK_NULL_HANDLE;
	pipelineLayout = VK_NULL_HANDLE;
	descriptorSet = VK_NULL_HANDLE;
	descriptorSetLayout = VK_NULL_HANDLE;
	_pool = VK_NULL_HANDLE;
}