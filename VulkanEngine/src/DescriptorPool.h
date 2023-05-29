#pragma once

#include "vulkan/vulkan.h"

#include "LogicalDevice.h"

namespace Renderer {

class DescriptorPool {
 public:
  DescriptorPool();
  ~DescriptorPool();

  VkResult Create(LogicalDevice* device, uint32_t max_sets);
  void Destroy();

  VkDescriptorPool GetPool();

  uint32_t GetMaxDescriptorCount(VkDescriptorType type) const;

  void SetMaxDescriptorCount(VkDescriptorType type, uint32_t max_count);

  VkResult AllocateDescriptorSet(VkDescriptorSetLayout layout,
                                 VkDescriptorSet* descriptor_sets,
                                 uint32_t count = 1);
  VkResult AllocateDescriptorSets(
      const std::vector<VkDescriptorSetLayout>& layouts,
      VkDescriptorSet* descriptor_sets);

  LogicalDevice* GetLogicalDevice();

 private:
  VkDescriptorPool descriptor_pool_;

  static const uint32_t kSizesCount = 11;
  std::array<uint32_t, kSizesCount> pool_sizes_;

  LogicalDevice* device_;
};

}