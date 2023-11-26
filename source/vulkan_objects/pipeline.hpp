#pragma once
#include "vulkan_api.hpp"
#include "descriptor_set.hpp"
#include "scope.hpp"

class Pipeline
{
public:
	Pipeline(const RenderScope& Scope);

	Pipeline(const Pipeline& other) = delete;

	void operator=(const Pipeline& other) = delete;

	Pipeline(Pipeline&& other) noexcept
		: Scope(other.Scope), pipeline(std::move(other.pipeline)), pipelineLayout(std::move(other.pipelineLayout))
	{
		other.pipeline = VK_NULL_HANDLE;
		other.pipelineLayout = VK_NULL_HANDLE;
	}

	void operator=(Pipeline&& other) noexcept {
		Scope = other.Scope;
		pipeline = std::move(other.pipeline);
		pipelineLayout = std::move(other.pipelineLayout);

		other.pipeline = VK_NULL_HANDLE;
		other.pipelineLayout = VK_NULL_HANDLE;
	}

	~Pipeline();

	Pipeline& SetVertexInputBindings(uint32_t count, VkVertexInputBindingDescription* bindings);

	Pipeline& SetVertexAttributeBindings(uint32_t count, VkVertexInputAttributeDescription* attributes);

	Pipeline& SetPrimitiveTopology(VkPrimitiveTopology topology);

	Pipeline& SetPolygonMode(VkPolygonMode mode);

	Pipeline& SetCullMode(VkCullModeFlags mode, VkFrontFace front = VK_FRONT_FACE_COUNTER_CLOCKWISE);

	Pipeline& SetDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor);

	Pipeline& SetBlendAttachments(uint32_t count, VkPipelineColorBlendAttachmentState* attachments);

	Pipeline& SetDepthState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable = VK_TRUE, VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL);

	Pipeline& SetSampling(VkSampleCountFlagBits samples);
	
	Pipeline& SetShaderStages(const std::string& shaderName, VkShaderStageFlagBits stages);

	Pipeline& AddDescriptorLayout(VkDescriptorSetLayout layout);

	Pipeline& SetSubpass(uint32_t subpass);

	void Construct();

	void BindPipeline(VkCommandBuffer cmd) const;

	void BindSet(VkCommandBuffer cmd, const DescriptorSet& set) const;

private:
	uint32_t subpass = 0;
	std::string shaderName = "";
	VkShaderStageFlagBits shaderStagesFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	const RenderScope* Scope;

	std::vector<VkDescriptorSetLayout> descriptorLayouts{};
	std::vector<VkPushConstantRange> pushConstants{};

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