#include "PipelineBarriers.h"

namespace Renderer {

BufferMemoryBarrier::BufferMemoryBarrier()
    : barrier_{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER} {}

BufferMemoryBarrier::BufferMemoryBarrier(BufferBase buffer)
    : barrier_{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER} {
  SetBuffer(buffer);
}

BufferMemoryBarrier::BufferMemoryBarrier(BufferBase buffer,
                                         uint32_t queue_family)
    : barrier_{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER} {
  SetBuffer(buffer);
  SetQueueFamily(queue_family);
}

BufferMemoryBarrier::BufferMemoryBarrier(BufferBase buffer,
                                         uint32_t src_queue_family,
                                         uint32_t dst_queue_family)
    : barrier_{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER} {
  SetBuffer(buffer);
  SetQueueFamily(src_queue_family, dst_queue_family);
}

void BufferMemoryBarrier::SetBuffer(BufferBase buffer) {
  barrier_.buffer = buffer.GetBuffer();
  barrier_.size = buffer.GetSize();
}

void BufferMemoryBarrier::SetQueueFamily(uint32_t queue_family) {
  barrier_.srcQueueFamilyIndex = queue_family;
  barrier_.dstQueueFamilyIndex = queue_family;
}

void BufferMemoryBarrier::SetQueueFamily(uint32_t src_queue_family,
                                         uint32_t dst_queue_family) {
  barrier_.srcQueueFamilyIndex = src_queue_family;
  barrier_.dstQueueFamilyIndex = dst_queue_family;
}

void BufferMemoryBarrier::SetSrcAccessMask(VkAccessFlags mask) {
  barrier_.srcAccessMask = mask;
}

void BufferMemoryBarrier::SetDstAccessMask(VkAccessFlags mask) {
  barrier_.dstAccessMask = mask;
}

void BufferMemoryBarrier::Use(CommandBuffer command_buffer,
                              VkPipelineStageFlags src_stage,
                              VkPipelineStageFlags dst_stage) {
  vkCmdPipelineBarrier(command_buffer.GetBuffer(), src_stage, dst_stage, 0, 0,
                       nullptr, 1, &barrier_, 0, nullptr);
}

VkBufferMemoryBarrier BufferMemoryBarrier::GetBarrier() {
  return barrier_;
}

}  // namespace Renderer