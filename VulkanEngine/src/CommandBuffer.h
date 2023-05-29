#pragma once

#include "vulkan/vulkan.hpp"

#include "LogicalDevice.h"

namespace Renderer {

class CommandPool;

class CommandBuffer {
 public:
  CommandBuffer(CommandPool* command_pool, uint32_t pool_index);
  CommandBuffer(CommandPool* command_pool, uint32_t pool_index,
                VkCommandBufferLevel level);
  ~CommandBuffer();

  VkCommandBuffer GetBuffer();
  VkCommandBuffer* GetBufferPtr();

  VkResult Reset();

  VkResult Begin(bool one_time = false);
  VkResult End();
  void Submit();
  void Submit(VkSubmitInfo submit_info);

  VkResult Create(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  void Destroy();

  static VkCommandBufferAllocateInfo AllocateInfo(
      VkCommandPool command_pool, uint32_t count = 1,
      VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

 private:
  VkCommandBuffer command_buffer_;

  uint32_t pool_index_;
  CommandPool* pool_;
};

}