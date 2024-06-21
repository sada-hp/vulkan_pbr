#pragma once
#include "vulkan_api.hpp"
#include "pipeline.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "scope.hpp"

class DescriptorSet
{
public:
	DescriptorSet(const RenderScope& Scope);

	~DescriptorSet();

	const VkDescriptorSetLayout& GetLayout() const { return descriptorSetLayout; };

	void BindSet(VkCommandBuffer cmd, const Pipeline& pipeline);

private:
	friend class DescriptorSetDescriptor;

	const RenderScope& Scope;

	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
};

class DescriptorSetDescriptor
{
public:
	DescriptorSetDescriptor() {};

	DescriptorSetDescriptor& AddUniformBuffer(uint32_t binding, VkShaderStageFlags stages, const Buffer& buffer);

	DescriptorSetDescriptor& AddStorageBuffer(uint32_t binding, VkShaderStageFlags stages, const Buffer& buffer);

	DescriptorSetDescriptor& AddImageSampler(uint32_t binding, VkShaderStageFlags stages, const VulkanImage& image);

	DescriptorSetDescriptor& AddSubpassAttachment(uint32_t binding, VkShaderStageFlags stages, const VulkanImage& image);

	DescriptorSetDescriptor& AddStorageImage(uint32_t binding, VkShaderStageFlags stages, const VulkanImage& image);

	TAuto<DescriptorSet> Allocate(const RenderScope& Scope);

private:
	TVector<VkDescriptorSetLayoutBinding> bindings;
	TVector<VkWriteDescriptorSet> writes;

	const RenderScope* Scope;
};