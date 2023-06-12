#pragma once

#include <vulkan/vulkan.hpp>

#include <unordered_map>
#include <vector>

#include "Vulkan/LogicalDevice.h"

namespace Renderer {

class DescriptorAllocator {
 public:
  struct PoolSizes {
    std::vector<std::pair<VkDescriptorType, float>> sizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}};
  };

  DescriptorAllocator();

  void ResetPools();
  bool Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);

  void Init(LogicalDevice* device);

  void Destroy();

  LogicalDevice* GetLogicalDevice();

 private:
  VkDescriptorPool GetPool();
  VkDescriptorPool CreatePool(size_t count, VkDescriptorPoolCreateFlags flags);

  VkDescriptorPool current_pool_;
  PoolSizes descriptor_sizes_;
  std::vector<VkDescriptorPool> used_pools_;
  std::vector<VkDescriptorPool> free_pools_;

  LogicalDevice* device_;
};

class DescriptorLayoutCache {
 public:
  void Init(LogicalDevice* device);
  void Destroy();

  VkDescriptorSetLayout CreateDescriptorLayout(
      VkDescriptorSetLayoutCreateInfo* info);

  struct DescriptorLayoutInfo {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    bool operator==(const DescriptorLayoutInfo& other) const;

    size_t hash() const;
  };

 private:
  struct DescriptorLayoutHash {
    size_t operator()(const DescriptorLayoutInfo& info) const {
      return info.hash();
    }
  };

  std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout,
                     DescriptorLayoutHash>
      layout_cache_;
  LogicalDevice* device_;
};

class DescriptorBuilder {
 public:
  static DescriptorBuilder Begin(DescriptorLayoutCache* cache,
                                 DescriptorAllocator* allocator);

  DescriptorBuilder& BindBuffer(uint32_t binding,
                                VkDescriptorBufferInfo* buffer_info,
                                VkDescriptorType type,
                                VkShaderStageFlags stage_flags);
  DescriptorBuilder& BindImage(uint32_t binding,
                                VkDescriptorImageInfo* image_info,
                                VkDescriptorType type,
                                VkShaderStageFlags stage_flags);

  bool Build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
  bool Build(VkDescriptorSet& set);

 private:
  std::vector<VkWriteDescriptorSet> writes_;
  std::vector<VkDescriptorSetLayoutBinding> bindings_;

  DescriptorLayoutCache* cache_;
  DescriptorAllocator* allocator_;
};

}