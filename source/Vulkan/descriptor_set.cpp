#include "descriptor_set.hpp"

DescriptorSet::DescriptorSet(const RenderScope& InScope)
	: Scope(InScope)
{

}

DescriptorSet::~DescriptorSet()
{
	vkDestroyDescriptorSetLayout(Scope.GetDevice(), descriptorSetLayout, VK_NULL_HANDLE);
	vkFreeDescriptorSets(Scope.GetDevice(), Scope.GetDescriptorPool(), 1, &descriptorSet);
}

void DescriptorSet::BindSet(uint32_t set, VkCommandBuffer cmd, const Pipeline& pipeline)
{
	vkCmdBindDescriptorSets(cmd, pipeline.GetBindPoint(), pipeline.GetLayout(), set, 1, &descriptorSet, 0, VK_NULL_HANDLE);
}

DescriptorSetDescriptor& DescriptorSetDescriptor::AddUniformBuffer(uint32_t binding, VkShaderStageFlags stages, const Buffer& buffer)
{
	VkDescriptorSetLayoutBinding DSBinding{};
	DSBinding.binding = binding;
	DSBinding.descriptorCount = 1;
	DSBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	DSBinding.stageFlags = stages;

	VkWriteDescriptorSet DSWrites{};
	DSWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DSWrites.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	DSWrites.descriptorCount = 1;
	DSWrites.dstBinding = binding;
	DSWrites.pBufferInfo = &buffer.GetDescriptor();

	bindings.push_back(DSBinding);
	writes.push_back(DSWrites);

	return *this;
}

DescriptorSetDescriptor& DescriptorSetDescriptor::AddStorageBuffer(uint32_t binding, VkShaderStageFlags stages, const Buffer& buffer)
{
	VkDescriptorSetLayoutBinding DSBinding{};
	DSBinding.binding = binding;
	DSBinding.descriptorCount = 1;
	DSBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	DSBinding.stageFlags = stages;

	VkWriteDescriptorSet DSWrites{};
	DSWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DSWrites.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	DSWrites.descriptorCount = 1;
	DSWrites.dstBinding = binding;
	DSWrites.pBufferInfo = &buffer.GetDescriptor();

	bindings.push_back(DSBinding);
	writes.push_back(DSWrites);

	return *this;
}

DescriptorSetDescriptor& DescriptorSetDescriptor::AddImageSampler(uint32_t binding, VkShaderStageFlags stages, const VkImageView& view, const VkSampler& sampler, VkImageLayout layout)
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

	imageInfos.emplace_back(sampler, view, layout);
	DSWrites.pImageInfo = &imageInfos.back();

	bindings.push_back(DSBinding);
	writes.push_back(DSWrites);

	return *this;
}

DescriptorSetDescriptor& DescriptorSetDescriptor::AddSubpassAttachment(uint32_t binding, VkShaderStageFlags stages, const VkImageView& view, VkImageLayout layout)
{
	VkDescriptorSetLayoutBinding DSBinding{};
	DSBinding.binding = binding;
	DSBinding.descriptorCount = 1;
	DSBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	DSBinding.stageFlags = stages;

	VkWriteDescriptorSet DSWrites{};
	DSWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DSWrites.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	DSWrites.descriptorCount = 1;
	DSWrites.dstBinding = binding;

	imageInfos.emplace_back(VK_NULL_HANDLE, view, layout);
	DSWrites.pImageInfo = &imageInfos.back();

	bindings.push_back(DSBinding);
	writes.push_back(DSWrites);

	return *this;
}

DescriptorSetDescriptor& DescriptorSetDescriptor::AddStorageImage(uint32_t binding, VkShaderStageFlags stages, const VkImageView& view, VkImageLayout layout)
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

	imageInfos.emplace_back(VK_NULL_HANDLE, view, layout);
	DSWrites.pImageInfo = &imageInfos.back();

	bindings.push_back(DSBinding);
	writes.push_back(DSWrites);

	return *this;
}

std::unique_ptr<DescriptorSet> DescriptorSetDescriptor::Allocate(const RenderScope& Scope)
{
	std::unique_ptr<DescriptorSet> out = std::make_unique<DescriptorSet>(Scope);

	VkDescriptorSetLayoutCreateInfo dsetInfo{};
	dsetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsetInfo.bindingCount = bindings.size();
	dsetInfo.pBindings = bindings.data();
	vkCreateDescriptorSetLayout(Scope.GetDevice(), &dsetInfo, VK_NULL_HANDLE, &out->descriptorSetLayout);

	VkDescriptorSetAllocateInfo setAlloc{};
	setAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAlloc.descriptorPool = Scope.GetDescriptorPool();
	setAlloc.descriptorSetCount = 1;
	setAlloc.pSetLayouts = &out->descriptorSetLayout;
	vkAllocateDescriptorSets(Scope.GetDevice(), &setAlloc, &out->descriptorSet);

	for (auto& write : writes)
		write.dstSet = out->descriptorSet;

	vkUpdateDescriptorSets(Scope.GetDevice(), writes.size(), writes.data(), 0, VK_NULL_HANDLE);

	return out;
}