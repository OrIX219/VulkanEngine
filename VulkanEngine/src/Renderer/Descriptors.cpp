#include "Descriptors.h"

#include <algorithm>

namespace Renderer {

DescriptorAllocator::DescriptorAllocator() : current_pool_(VK_NULL_HANDLE) {}

void DescriptorAllocator::ResetPools() {
  for (auto pool : used_pools_) {
    vkResetDescriptorPool(device_->GetDevice(), pool, 0);
    free_pools_.push_back(pool);
  }

  used_pools_.clear();

  current_pool_ = VK_NULL_HANDLE;
}

bool DescriptorAllocator::Allocate(VkDescriptorSet* set,
                                   VkDescriptorSetLayout layout) {
  if (current_pool_ == VK_NULL_HANDLE) {
    current_pool_ = GetPool();
    used_pools_.push_back(current_pool_);
  }

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.pSetLayouts = &layout;
  alloc_info.descriptorSetCount = 1;
  alloc_info.descriptorPool = current_pool_;

  VkResult alloc_result =
      vkAllocateDescriptorSets(device_->GetDevice(), &alloc_info, set);
  bool need_reallocate = false;

  switch (alloc_result) {
    case VK_SUCCESS:
      return true;
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
      need_reallocate = true;
      break;
    default:
      return false;
  }

  if (need_reallocate) {
    current_pool_ = GetPool();
    used_pools_.push_back(current_pool_);
    alloc_info.descriptorPool = current_pool_;

    alloc_result =
        vkAllocateDescriptorSets(device_->GetDevice(), &alloc_info, set);
  }

  return alloc_result == VK_SUCCESS;
}
void DescriptorAllocator::Init(LogicalDevice* device) { device_ = device; }

void DescriptorAllocator::Destroy() {
  for (auto pool : free_pools_)
    vkDestroyDescriptorPool(device_->GetDevice(), pool, nullptr);
  for (auto pool : used_pools_)
    vkDestroyDescriptorPool(device_->GetDevice(), pool, nullptr);
}

LogicalDevice* DescriptorAllocator::GetLogicalDevice() { return device_; }

VkDescriptorPool DescriptorAllocator::GetPool() {
  if (free_pools_.size() > 0) {
    VkDescriptorPool pool = free_pools_.back();
    free_pools_.pop_back();
    return pool;
  } else {
    return CreatePool(1000, 0);
  }
}

VkDescriptorPool DescriptorAllocator::CreatePool(
    size_t count, VkDescriptorPoolCreateFlags flags) {
  std::vector<VkDescriptorPoolSize> sizes;
  sizes.reserve(descriptor_sizes_.sizes.size());
  for (auto size : descriptor_sizes_.sizes)
    sizes.push_back({size.first, static_cast<uint32_t>(size.second * count)});

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = static_cast<uint32_t>(sizes.size());
  pool_info.pPoolSizes = sizes.data();
  pool_info.maxSets = static_cast<uint32_t>(count);
  pool_info.flags = flags;

  VkDescriptorPool descriptor_pool;
  vkCreateDescriptorPool(device_->GetDevice(), &pool_info, nullptr,
                         &descriptor_pool);

  return descriptor_pool;
}

void DescriptorLayoutCache::Init(LogicalDevice* device) { device_ = device; }

void DescriptorLayoutCache::Destroy() {
  for (auto layout : layout_cache_)
    vkDestroyDescriptorSetLayout(device_->GetDevice(), layout.second, nullptr);
}

VkDescriptorSetLayout DescriptorLayoutCache::CreateDescriptorLayout(
    VkDescriptorSetLayoutCreateInfo* info) {
  DescriptorLayoutInfo layout_info;
  layout_info.bindings.reserve(info->bindingCount);
  bool is_sorted = true;
  int last_binding = -1;

  for (size_t i = 0; i < info->bindingCount; ++i) {
    layout_info.bindings.push_back(info->pBindings[i]);

    if (info->pBindings[i].binding > last_binding)
      last_binding = info->pBindings[i].binding;
    else
      is_sorted = false;
  }

  if (!is_sorted) {
    std::sort(layout_info.bindings.begin(), layout_info.bindings.end(),
              [](VkDescriptorSetLayoutBinding& lhs,
                 VkDescriptorSetLayoutBinding& rhs) {
                return lhs.binding < rhs.binding;
              });
  }

  auto iter = layout_cache_.find(layout_info);
  if (iter != layout_cache_.end()) {
    return iter->second;
  } else {
    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(device_->GetDevice(), info, nullptr, &layout);

    layout_cache_[layout_info] = layout;
    return layout;
  }
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(
    const DescriptorLayoutInfo& other) const {
  if (other.bindings.size() != bindings.size()) return false;
  for (size_t i = 0; i < bindings.size(); ++i) {
    const VkDescriptorSetLayoutBinding& other_binding = other.bindings[i]; 
    const VkDescriptorSetLayoutBinding& binding = bindings[i]; 
    if (binding.binding != other_binding.binding) return false;
    if (binding.descriptorType != other_binding.descriptorType) return false;
    if (binding.descriptorCount != other_binding.descriptorCount) return false;
    if (binding.stageFlags != other_binding.stageFlags) return false;
  }
  return true;
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const {
  size_t result = std::hash<size_t>()(bindings.size());

  for (const VkDescriptorSetLayoutBinding& binding : bindings) {
    size_t binding_hash = binding.binding | binding.descriptorType << 8 |
                          binding.descriptorCount << 16 |
                          binding.stageFlags << 24;
    result ^= std::hash<size_t>()(binding_hash);
  }

  return result;
}

DescriptorBuilder DescriptorBuilder::Begin(DescriptorLayoutCache* cache,
                                           DescriptorAllocator* allocator) {
  DescriptorBuilder builder;
  builder.cache_ = cache;
  builder.allocator_ = allocator;

  return builder;
}

DescriptorBuilder& DescriptorBuilder::BindBuffer(
    uint32_t binding, VkDescriptorBufferInfo* buffer_info,
    VkDescriptorType type, VkShaderStageFlags stage_flags) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.descriptorCount = 1;
  new_binding.descriptorType = type;
  new_binding.stageFlags = stage_flags;
  new_binding.binding = binding;

  bindings_.push_back(new_binding);

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pBufferInfo = buffer_info;
  write.dstBinding = binding;

  writes_.push_back(write);

  return *this;
}

DescriptorBuilder& DescriptorBuilder::BindImage(
    uint32_t binding, VkDescriptorImageInfo* image_info, VkDescriptorType type,
    VkShaderStageFlags stage_flags) {
  VkDescriptorSetLayoutBinding new_binding{};
  new_binding.descriptorCount = 1;
  new_binding.descriptorType = type;
  new_binding.stageFlags = stage_flags;
  new_binding.binding = binding;

  bindings_.push_back(new_binding);

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pImageInfo = image_info;
  write.dstBinding = binding;

  writes_.push_back(write);

  return *this;
}

bool DescriptorBuilder::Build(VkDescriptorSet& set,
                              VkDescriptorSetLayout& layout) {
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(bindings_.size());
  layout_info.pBindings = bindings_.data();

  layout = cache_->CreateDescriptorLayout(&layout_info);

  bool success = allocator_->Allocate(&set, layout);
  if (!success) return false;

  for (VkWriteDescriptorSet& write : writes_) write.dstSet = set;

  vkUpdateDescriptorSets(allocator_->GetLogicalDevice()->GetDevice(),
                         static_cast<uint32_t>(writes_.size()), writes_.data(),
                         0, nullptr);

  return true;
}

bool DescriptorBuilder::Build(VkDescriptorSet& set) {
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(bindings_.size());
  layout_info.pBindings = bindings_.data();

  VkDescriptorSetLayout layout = cache_->CreateDescriptorLayout(&layout_info);

  bool success = allocator_->Allocate(&set, layout);
  if (!success) return false;

  for (VkWriteDescriptorSet& write : writes_) write.dstSet = set;

  vkUpdateDescriptorSets(allocator_->GetLogicalDevice()->GetDevice(),
                         static_cast<uint32_t>(writes_.size()), writes_.data(),
                         0, nullptr);

  return true;
}

}  // namespace Renderer