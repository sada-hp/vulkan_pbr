#include "descriptor_set.hpp"

DescriptorSet::DescriptorSet(const RenderScope& InScope)
	: Scope(&InScope)
{
}

DescriptorSet::~DescriptorSet()
{
	vkDestroyDescriptorSetLayout(Scope->GetDevice(), descriptorSetLayout, VK_NULL_HANDLE);
	vkFreeDescriptorSets(Scope->GetDevice(), Scope->GetDescriptorPool(), 1, &descriptorSet);
}

DescriptorSet& DescriptorSet::AddUniformBuffer(uint32_t binding, VkShaderStageFlags stages, const Buffer& buffer)
{
	VkDescriptorSetLayoutBinding DSBinding{};
	DSBinding.binding = binding;
	DSBinding.descriptorCount = 1;
	DSBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	DSBinding.stageFlags = stages;

	VkWriteDescriptorSet DSWrites{};
	DSWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DSWrites.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	DSWrites.dstSet = descriptorSet;
	DSWrites.descriptorCount = 1;
	DSWrites.dstBinding = binding;
	DSWrites.pBufferInfo = &buffer.GetDescriptor();

	bindings.push_back(DSBinding);
	writes.push_back(DSWrites);

	return *this;
}

DescriptorSet& DescriptorSet::AddImageSampler(uint32_t binding, VkShaderStageFlags stages, const Image& image)
{
	VkDescriptorSetLayoutBinding DSBinding{};
	DSBinding.binding = binding;
	DSBinding.descriptorCount = 1;
	DSBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	DSBinding.stageFlags = stages;

	VkWriteDescriptorSet DSWrites{};
	DSWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DSWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	DSWrites.descriptorCount = 1;
	DSWrites.dstBinding = binding;
	DSWrites.pImageInfo = &image.GetDescriptor();

	bindings.push_back(DSBinding);
	writes.push_back(DSWrites);

	return *this;
}

DescriptorSet& DescriptorSet::AddStorageImage(uint32_t binding, VkShaderStageFlags stages, const Image& image)
{
	VkDescriptorSetLayoutBinding DSBinding{};
	DSBinding.binding = binding;
	DSBinding.descriptorCount = 1;
	DSBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	DSBinding.stageFlags = stages;

	VkWriteDescriptorSet DSWrites{};
	DSWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DSWrites.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	DSWrites.descriptorCount = 1;
	DSWrites.dstBinding = binding;
	DSWrites.pImageInfo = &image.GetDescriptor();

	bindings.push_back(DSBinding);
	writes.push_back(DSWrites);

	return *this;
}

void DescriptorSet::Allocate()
{
	if (descriptorSet != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(Scope->GetDevice(), descriptorSetLayout, VK_NULL_HANDLE);
		vkFreeDescriptorSets(Scope->GetDevice(), Scope->GetDescriptorPool(), 1, &descriptorSet);
	}

	VkDescriptorSetLayoutCreateInfo dsetInfo{};
	dsetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsetInfo.bindingCount = bindings.size();
	dsetInfo.pBindings = bindings.data();

	VkDescriptorSetAllocateInfo setAlloc{};
	setAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAlloc.descriptorPool = Scope->GetDescriptorPool();
	setAlloc.descriptorSetCount = 1;
	setAlloc.pSetLayouts = &descriptorSetLayout;

	vkCreateDescriptorSetLayout(Scope->GetDevice(), &dsetInfo, VK_NULL_HANDLE, &descriptorSetLayout);
	vkAllocateDescriptorSets(Scope->GetDevice(), &setAlloc, &descriptorSet);

	for (auto& write : writes)
		write.dstSet = descriptorSet;

	vkUpdateDescriptorSets(Scope->GetDevice(), writes.size(), writes.data(), 0, VK_NULL_HANDLE);
}

void DescriptorSet::BindSet(VkCommandBuffer cmd, const Pipeline& pipeline)
{
	vkCmdBindDescriptorSets(cmd, pipeline.GetBindPoint(), pipeline.GetLayout(), 0, 1, &descriptorSet, 0, VK_NULL_HANDLE);
}