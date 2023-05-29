#include "DescriptorPool.h"

namespace Renderer {

DescriptorPool::DescriptorPool()
    : pool_sizes_{0}, descriptor_pool_(VK_NULL_HANDLE) {}

DescriptorPool::~DescriptorPool() {}

VkResult DescriptorPool::Create(LogicalDevice* device, uint32_t max_sets) {
  device_ = device;

  std::vector<VkDescriptorPoolSize> pool_sizes{};
  pool_sizes.reserve(kSizesCount);
  for (size_t i = 0; i < pool_sizes_.size(); ++i) {
    if (pool_sizes_[i] > 0)
      pool_sizes.push_back({static_cast<VkDescriptorType>(i), pool_sizes_[i]});
  }

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();
  pool_info.maxSets = max_sets;

  return vkCreateDescriptorPool(device_->GetDevice(), &pool_info, nullptr,
                                &descriptor_pool_);
}

void DescriptorPool::Destroy() {
  if (descriptor_pool_ == VK_NULL_HANDLE) return;
  vkDestroyDescriptorPool(device_->GetDevice(), descriptor_pool_, nullptr);
}

VkDescriptorPool DescriptorPool::GetPool() { return descriptor_pool_; }

uint32_t DescriptorPool::GetMaxDescriptorCount(VkDescriptorType type) const {
  return pool_sizes_.at(static_cast<uint32_t>(type));
}

void DescriptorPool::SetMaxDescriptorCount(VkDescriptorType type,
                                           uint32_t max_count) {
  pool_sizes_[static_cast<uint32_t>(type)] = max_count;
}

VkResult DescriptorPool::AllocateDescriptorSet(VkDescriptorSetLayout layout,
                                           VkDescriptorSet* descriptor_sets,
                                           uint32_t count) {
  std::vector<VkDescriptorSetLayout> layouts(count, layout);
  return AllocateDescriptorSets(layouts, descriptor_sets);
}

VkResult DescriptorPool::AllocateDescriptorSets(
    const std::vector<VkDescriptorSetLayout>& layouts,
    VkDescriptorSet* descriptor_sets) {
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool_;
  alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
  alloc_info.pSetLayouts = layouts.data();

  return vkAllocateDescriptorSets(device_->GetDevice(), &alloc_info,
                                  descriptor_sets);
}

LogicalDevice* DescriptorPool::GetLogicalDevice() { return device_; }

}  // namespace Renderer