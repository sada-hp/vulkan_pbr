#pragma once
#include "vulkan_api.hpp"
#include "Vulkan/pipeline.hpp"
#include "Vulkan/buffer.hpp"
#include "Vulkan/image.hpp"
#include "Vulkan/scope.hpp"

class DescriptorSet
{
public:
	DescriptorSet(const RenderScope& Scope);

	~DescriptorSet();

	const VkDescriptorSetLayout& GetLayout() const { return descriptorSetLayout; };

	void BindSet(uint32_t set, VkCommandBuffer cmd, const Pipeline& pipeline);

private:
	friend class DescriptorSetDescriptor;

	const RenderScope& Scope;

	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
};

class DescriptorSetDescriptor
{
public:
	DescriptorSetDescriptor() 
	{
		imageInfos.reserve(32);
	};

	DescriptorSetDescriptor& AddUniformBuffer(uint32_t binding, VkShaderStageFlags stages, const Buffer& buffer);

	DescriptorSetDescriptor& AddStorageBuffer(uint32_t binding, VkShaderStageFlags stages, const Buffer& buffer);

	DescriptorSetDescriptor& AddImageSampler(uint32_t binding, VkShaderStageFlags stages, const VkImageView& view, const VkSampler& sampler);

	DescriptorSetDescriptor& AddSubpassAttachment(uint32_t binding, VkShaderStageFlags stages, const VkImageView& view);

	DescriptorSetDescriptor& AddStorageImage(uint32_t binding, VkShaderStageFlags stages, const VkImageView& view);

	TAuto<DescriptorSet> Allocate(const RenderScope& Scope);

private:
	TVector<VkDescriptorSetLayoutBinding> bindings;
	TVector<VkWriteDescriptorSet> writes;

	std::vector<VkDescriptorImageInfo> imageInfos;

	const RenderScope* Scope = VK_NULL_HANDLE;
};