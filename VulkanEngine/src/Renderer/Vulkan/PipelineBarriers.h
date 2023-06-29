#pragma once

#include <vulkan/vulkan.hpp>

#include "Buffer.h"
#include "CommandBuffer.h"

namespace Renderer {

class BufferMemoryBarrier {
 public:
  BufferMemoryBarrier();
  BufferMemoryBarrier(BufferBase buffer);
  BufferMemoryBarrier(BufferBase buffer, uint32_t queue_family);
  BufferMemoryBarrier(BufferBase buffer, uint32_t src_queue_family,
                      uint32_t dst_queue_family);

  void SetBuffer(BufferBase buffer);
  void SetQueueFamily(uint32_t queue_family);
  void SetQueueFamily(uint32_t src_queue_family, uint32_t dst_queue_family);
  void SetSrcAccessMask(VkAccessFlags mask);
  void SetDstAccessMask(VkAccessFlags mask);

  void Use(CommandBuffer command_buffer, VkPipelineStageFlags src_stage,
           VkPipelineStageFlags dst_stage);

  VkBufferMemoryBarrier Get();

 private:
  VkBufferMemoryBarrier barrier_;
};

}