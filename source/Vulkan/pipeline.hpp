#pragma once
#include "Vulkan/vulkan_api.hpp"
#include "Vulkan/scope.hpp"

class ComputePipeline
{
	friend class ComputePipelineDescriptor;

public:
	ComputePipeline(const RenderScope& Scope) : scope(Scope) {};

	virtual ~ComputePipeline();

	ComputePipeline& BindPipeline(VkCommandBuffer cmd);

	ComputePipeline& PushConstants(VkCommandBuffer cmd, const void* data, size_t dataSize, size_t offset, VkShaderStageFlagBits stages);

	const VkPipelineLayout& GetLayout() const { return pipelineLayout; };

private:
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	const RenderScope& scope;
};

class GraphicsPipeline
{
	friend class GraphicsPipelineDescriptor;

public:
	GraphicsPipeline(const RenderScope& Scope) : scope(Scope) {};

	virtual ~GraphicsPipeline();

	GraphicsPipeline& BindPipeline(VkCommandBuffer cmd);

	GraphicsPipeline& PushConstants(VkCommandBuffer cmd, const void* data, size_t dataSize, size_t offset, VkShaderStageFlagBits stages);

	const VkPipelineLayout& GetLayout() const { return pipelineLayout; };

private:
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	const RenderScope& scope;
};

class PipelineDescriptor
{
public:
	PipelineDescriptor();

	virtual ~PipelineDescriptor() {};

	PipelineDescriptor(const PipelineDescriptor& other) = delete;

	void operator=(const PipelineDescriptor& other) = delete;

	PipelineDescriptor(PipelineDescriptor&& other) noexcept
		: specializationConstants(std::move(other.specializationConstants)),
		pushConstants(std::move(other.pushConstants)), descriptorLayouts(std::move(other.descriptorLayouts))
	{
		other.pushConstants.clear();
		other.descriptorLayouts.clear();
		other.specializationConstants.clear();
	}

	void operator=(PipelineDescriptor&& other) noexcept {
		pushConstants = std::move(other.pushConstants);
		descriptorLayouts = std::move(other.descriptorLayouts);
		specializationConstants = std::move(other.specializationConstants);

		other.pushConstants.clear();
		other.descriptorLayouts.clear();
		other.specializationConstants.clear();
	}

protected:
	std::vector<VkDescriptorSetLayout> descriptorLayouts{};
	std::vector<VkPushConstantRange> pushConstants{};
	std::map<VkShaderStageFlagBits, std::map<uint32_t, std::any>> specializationConstants;
	// std::map<uint32_t, std::map<VkShaderStageFlagBits, std::any>> specializationConstants;
	std::map<VkShaderStageFlagBits, std::string> shaderNames;
};

class ComputePipelineDescriptor : public PipelineDescriptor
{
public:
	ComputePipelineDescriptor();

	ComputePipelineDescriptor(const ComputePipelineDescriptor& other) = delete;

	void operator=(const ComputePipelineDescriptor& other) = delete;

	ComputePipelineDescriptor(ComputePipelineDescriptor&& other) noexcept
		: PipelineDescriptor(std::move(other))
	{
	}

	void operator=(ComputePipelineDescriptor&& other) noexcept {
		PipelineDescriptor::operator=(std::move(other));
	}

	~ComputePipelineDescriptor();

	ComputePipelineDescriptor& SetShaderName(const std::string& shaderName);

	ComputePipelineDescriptor& AddDescriptorLayout(VkDescriptorSetLayout layout);

	ComputePipelineDescriptor& AddPushConstant(VkPushConstantRange constantRange);

	ComputePipelineDescriptor& AddSpecializationConstant(uint32_t id, std::any value);

	std::unique_ptr<ComputePipeline> Construct(const RenderScope& Scope);
};

class GraphicsPipelineDescriptor : public PipelineDescriptor
{
public:
	GraphicsPipelineDescriptor();

	GraphicsPipelineDescriptor(const GraphicsPipelineDescriptor& other) = delete;

	void operator=(const GraphicsPipelineDescriptor& other) = delete;

	GraphicsPipelineDescriptor(GraphicsPipelineDescriptor&& other) noexcept
		: PipelineDescriptor(std::move(other))
	{
	}

	void operator=(GraphicsPipelineDescriptor&& other) noexcept {
		PipelineDescriptor::operator=(std::move(other));
	}

	~GraphicsPipelineDescriptor();

	GraphicsPipelineDescriptor& SetVertexInputBindings(uint32_t count, VkVertexInputBindingDescription* bindings);

	GraphicsPipelineDescriptor& SetVertexAttributeBindings(uint32_t count, VkVertexInputAttributeDescription* attributes);

	GraphicsPipelineDescriptor& SetPrimitiveTopology(VkPrimitiveTopology topology);

	GraphicsPipelineDescriptor& SetPolygonMode(VkPolygonMode mode);

	GraphicsPipelineDescriptor& SetCullMode(VkCullModeFlags mode, VkFrontFace front = VK_FRONT_FACE_COUNTER_CLOCKWISE);

	GraphicsPipelineDescriptor& SetDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor);

	GraphicsPipelineDescriptor& SetBlendAttachments(uint32_t count, VkPipelineColorBlendAttachmentState* attachments);

	GraphicsPipelineDescriptor& SetDepthState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable = VK_TRUE, VkCompareOp depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL);

	GraphicsPipelineDescriptor& SetSampling(VkSampleCountFlagBits samples);
	
	GraphicsPipelineDescriptor& SetShaderStage(const std::string& shaderName, VkShaderStageFlagBits stage);

	GraphicsPipelineDescriptor& AddDescriptorLayout(VkDescriptorSetLayout layout);

	GraphicsPipelineDescriptor& AddPushConstant(VkPushConstantRange constantRange);

	GraphicsPipelineDescriptor& AddSpecializationConstant(uint32_t id, std::any value, VkShaderStageFlagBits stage);

	GraphicsPipelineDescriptor& SetRenderPass(const VkRenderPass& RenderPass, uint32_t Subpass);

	std::unique_ptr<GraphicsPipeline> Construct(const RenderScope& Scope);

private:
	uint32_t subpass = 0;
	VkRenderPass renderPass = VK_NULL_HANDLE;

	VkPipelineVertexInputStateCreateInfo vertexInput{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0u,
		0u,
		VK_NULL_HANDLE,
		0u,
		VK_NULL_HANDLE
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0u,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE
	};

	VkPipelineRasterizationStateCreateInfo rasterizationState{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0u,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_NONE,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE,
		0.f,
		0.f,
		0.f,
		1.f
	};

	const VkPipelineColorBlendAttachmentState defaultAttachment{
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments{ 
		defaultAttachment
	};

	VkPipelineColorBlendStateCreateInfo blendState{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0u,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		static_cast<uint32_t>(blendAttachments.size()),
		blendAttachments.data(),
		0.f
	};

	const VkStencilOpState defaultStencil{
		VK_STENCIL_OP_KEEP,
		VK_STENCIL_OP_KEEP,
		VK_STENCIL_OP_KEEP,
		VK_COMPARE_OP_LESS_OR_EQUAL,
		0xff,
		0x0,
		0x0
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilState{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0u,
		VK_TRUE,
		VK_TRUE,
		VK_COMPARE_OP_GREATER_OR_EQUAL,
		VK_FALSE,
		VK_FALSE,
		defaultStencil,
		defaultStencil,
		0.f,
		1.f
	};

	VkPipelineViewportStateCreateInfo viewportState{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0u,
		1u,
		VK_NULL_HANDLE,
		1u,
		VK_NULL_HANDLE
	};

	const std::array<VkDynamicState, 2> dynamics{ 
		VK_DYNAMIC_STATE_VIEWPORT, 
		VK_DYNAMIC_STATE_SCISSOR 
	};

	VkPipelineDynamicStateCreateInfo dynamicState{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		static_cast<uint32_t>(dynamics.size()),
		dynamics.data()
	};

	VkPipelineMultisampleStateCreateInfo multisampleState{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0u,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		0.f,
		VK_NULL_HANDLE,
		VK_FALSE,
		VK_FALSE
	};
};