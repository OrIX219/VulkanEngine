#pragma once

#include "vulkan/vulkan.hpp"

#include <vector>

#include "CommandBuffer.h"
#include "LogicalDevice.h"

namespace Renderer {

class CommandPool {
  friend class CommandBuffer;
 public:
  CommandPool();
  ~CommandPool();

  VkResult Create(LogicalDevice* device, uint32_t queue_family_index,
                  VkCommandPoolCreateFlags flags = 0);
  VkResult Create(LogicalDevice* device, VkCommandPoolCreateInfo* create_info);
  void Destroy();

  VkCommandPool GetPool() const; 

  size_t GetBuffersCount() const;

  /*
  Get command buffer from free pool

  - Allocates new command buffer if none is available
  */
  CommandBuffer GetBuffer();

  /*
  Allocate new command buffer and add to free pool
  */
  void CreateBuffer(
      VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  void Clear();
  VkResult Reset();

  uint32_t GetQueueFamily() const;
  LogicalDevice* GetLogicalDevice();

  static VkCommandPoolCreateInfo CreateInfo(uint32_t queue_family,
                                            VkCommandPoolCreateFlags flags = 0);

 private:
  VkCommandPool command_pool_;
  uint32_t queue_family_;
  VkCommandPoolCreateFlags flags_;

  std::vector<CommandBuffer> command_buffers_;
  size_t available_index_;

  LogicalDevice* device_;
};

}