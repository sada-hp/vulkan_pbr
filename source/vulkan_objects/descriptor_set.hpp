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

	DescriptorSet(const DescriptorSet& other) = delete;

	void operator=(const DescriptorSet& other) = delete;

	DescriptorSet(DescriptorSet&& other) noexcept
		: Scope(other.Scope), descriptorSet(other.descriptorSet), descriptorSetLayout(other.descriptorSetLayout)
	{
		other.descriptorSet = VK_NULL_HANDLE;
		other.descriptorSetLayout = VK_NULL_HANDLE;
	}

	void operator=(DescriptorSet&& other) noexcept
	{
		descriptorSet = other.descriptorSet;
		descriptorSetLayout = other.descriptorSetLayout;
		other.descriptorSet = VK_NULL_HANDLE;
		other.descriptorSetLayout = VK_NULL_HANDLE;
	}

	~DescriptorSet();

	DescriptorSet& AddUniformBuffer(uint32_t binding, VkShaderStageFlags stages, const Buffer& buffer);

	DescriptorSet& AddImageSampler(uint32_t binding, VkShaderStageFlags stages, const Image& image);

	DescriptorSet& AddSubpassAttachment(uint32_t binding, VkShaderStageFlags stages, const Image& image);

	DescriptorSet& AddStorageImage(uint32_t binding, VkShaderStageFlags stages, const Image& image);

	void Allocate();

	void BindSet(VkCommandBuffer cmd, const Pipeline& pipeline);

	const VkDescriptorSetLayout& GetLayout() const { return descriptorSetLayout; };

private:
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	TVector<VkDescriptorSetLayoutBinding> bindings;
	TVector<VkWriteDescriptorSet> writes;

	const RenderScope* Scope;
};