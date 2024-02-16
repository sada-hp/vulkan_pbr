#pragma once
#include "vulkan_api.hpp"
#include "scope.hpp"

enum class EPipelineType
{
	None = 0,
	Graphics = 1,
	Compute = 2
};

//Use some pattern to get rid of useless structs after construction?
class Pipeline
{
public:
	Pipeline(const RenderScope& Scope);

	virtual ~Pipeline();

	Pipeline(const Pipeline& other) = delete;

	void operator=(const Pipeline& other) = delete;

	Pipeline(Pipeline&& other) noexcept
		: Scope(other.Scope), pipeline(std::move(other.pipeline)), pipelineLayout(std::move(other.pipelineLayout)),
		specializationConstants(std::move(other.specializationConstants)), shaderName(std::move(other.shaderName)),
		pushConstants(std::move(other.pushConstants)), descriptorLayouts(std::move(other.descriptorLayouts))
	{
		other.shaderName = "";
		other.pushConstants.clear();
		other.descriptorLayouts.clear();
		other.specializationConstants.clear();
		other.pipeline = VK_NULL_HANDLE;
		other.pipelineLayout = VK_NULL_HANDLE;
	}

	void operator=(Pipeline&& other) noexcept {
		Scope = other.Scope;
		pipeline = std::move(other.pipeline);
		pipelineLayout = std::move(other.pipelineLayout);

		pushConstants = std::move(other.pushConstants);
		descriptorLayouts = std::move(other.descriptorLayouts);
		specializationConstants = std::move(other.specializationConstants);

		shaderName = std::move(other.shaderName);

		other.shaderName = "";
		other.pushConstants.clear();
		other.descriptorLayouts.clear();
		other.specializationConstants.clear();
		other.pipeline = VK_NULL_HANDLE;
		other.pipelineLayout = VK_NULL_HANDLE;
	}

	virtual Pipeline& BindPipeline(VkCommandBuffer cmd) = 0;

	virtual const VkPipelineBindPoint GetBindPoint() const { return VK_PIPELINE_BIND_POINT_MAX_ENUM; };

	virtual EPipelineType GetPipelineType() const { return EPipelineType::None; };

	virtual const VkPipelineLayout& GetLayout() const { return pipelineLayout; };

protected:
	std::string shaderName = "";
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	const RenderScope* Scope;

	TVector<VkDescriptorSetLayout> descriptorLayouts{};
	TVector<VkPushConstantRange> pushConstants{};
	std::map<VkShaderStageFlagBits, std::map<uint32_t, std::any>> specializationConstants;
};

class ComputePipeline : public Pipeline
{
public:
	ComputePipeline(const RenderScope& Scope);

	ComputePipeline(const ComputePipeline& other) = delete;

	void operator=(const ComputePipeline& other) = delete;

	ComputePipeline(ComputePipeline&& other) noexcept
		: Pipeline(std::move(other))
	{
	}

	void operator=(ComputePipeline&& other) noexcept {
		Pipeline::operator=(std::move(other));
	}

	~ComputePipeline();

	ComputePipeline& SetShaderName(const std::string& shaderName);

	ComputePipeline& AddDescriptorLayout(VkDescriptorSetLayout layout);

	ComputePipeline& AddPushConstant(VkPushConstantRange constantRange);

	ComputePipeline& AddSpecializationConstant(uint32_t id, std::any value);

	void Construct();

	ComputePipeline& BindPipeline(VkCommandBuffer cmd) override;

	ComputePipeline& PushConstants(VkCommandBuffer cmd, const void* data);

	void Dispatch(VkCommandBuffer cmd, uint32_t size_x, uint32_t size_y, uint32_t size_z);

	const VkPipelineBindPoint GetBindPoint() const override { return VK_PIPELINE_BIND_POINT_COMPUTE; };

	virtual EPipelineType GetPipelineType() const override { return EPipelineType::Compute; };
private:
	uint32_t x_batch_size = 1;
	uint32_t y_batch_size = 1;
	uint32_t z_batch_size = 1;
};

class GraphicsPipeline : public Pipeline
{
public:
	GraphicsPipeline(const RenderScope& Scope);

	GraphicsPipeline(const GraphicsPipeline& other) = delete;

	void operator=(const GraphicsPipeline& other) = delete;

	GraphicsPipeline(GraphicsPipeline&& other) noexcept
		: Pipeline(std::move(other))
	{
	}

	void operator=(GraphicsPipeline&& other) noexcept {
		Pipeline::operator=(std::move(other));
	}

	~GraphicsPipeline();

	GraphicsPipeline& SetVertexInputBindings(uint32_t count, VkVertexInputBindingDescription* bindings);

	GraphicsPipeline& SetVertexAttributeBindings(uint32_t count, VkVertexInputAttributeDescription* attributes);

	GraphicsPipeline& SetPrimitiveTopology(VkPrimitiveTopology topology);

	GraphicsPipeline& SetPolygonMode(VkPolygonMode mode);

	GraphicsPipeline& SetCullMode(VkCullModeFlags mode, VkFrontFace front = VK_FRONT_FACE_COUNTER_CLOCKWISE);

	GraphicsPipeline& SetDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor);

	GraphicsPipeline& SetBlendAttachments(uint32_t count, VkPipelineColorBlendAttachmentState* attachments);

	GraphicsPipeline& SetDepthState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable = VK_TRUE, VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL);

	GraphicsPipeline& SetSampling(VkSampleCountFlagBits samples);
	
	GraphicsPipeline& SetShaderStages(const std::string& shaderName, VkShaderStageFlagBits stages);

	GraphicsPipeline& SetSubpass(uint32_t subpass);

	GraphicsPipeline& AddDescriptorLayout(VkDescriptorSetLayout layout);

	GraphicsPipeline& AddPushConstant(VkPushConstantRange constantRange);

	GraphicsPipeline& AddSpecializationConstant(uint32_t id, std::any value, VkShaderStageFlagBits stage);

	void Construct();

	GraphicsPipeline& BindPipeline(VkCommandBuffer cmd) override;

	GraphicsPipeline& PushConstants(VkCommandBuffer cmd, const void* data, size_t dataSize, size_t offset, VkShaderStageFlagBits stages);

	const VkPipelineBindPoint GetBindPoint() const override { return VK_PIPELINE_BIND_POINT_GRAPHICS; };

	virtual EPipelineType GetPipelineType() const override { return EPipelineType::Graphics; };

private:
	uint32_t subpass = 0;
	VkShaderStageFlagBits shaderStagesFlags = VK_SHADER_STAGE_ALL_GRAPHICS;

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

	TVector<VkPipelineColorBlendAttachmentState> blendAttachments{ 
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
		VK_COMPARE_OP_LESS_OR_EQUAL,
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

	const TArray<VkDynamicState, 2> dynamics{ 
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