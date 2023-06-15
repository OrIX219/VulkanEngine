#include "PushBuffer.h"

namespace Renderer {

PushBuffer::PushBuffer() {}

void PushBuffer::Create(VmaAllocator allocator, uint64_t size,
                        uint32_t alignment) {
  align_ = alignment;
  current_offset_ = 0;
  buffer_.Create(allocator, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
}

void PushBuffer::Destroy() { buffer_.Destroy(); }

uint32_t PushBuffer::Push(void* data, uint32_t size) {
  uint32_t offset = current_offset_;
  buffer_.SetData(data, size, current_offset_);
  current_offset_ += size;
  current_offset_ = PadUniformBufferSize(current_offset_);
    
  return offset;
}

void PushBuffer::Reset() { current_offset_ = 0; }

VkBuffer PushBuffer::GetBuffer() { return buffer_.GetBuffer(); }

VkDeviceSize PushBuffer::GetSize() const { return buffer_.GetSize(); }

uint32_t PushBuffer::PadUniformBufferSize(uint32_t original_size) {
  size_t min_ubo_alignment = align_;
  size_t aligned_size = original_size;
  if (min_ubo_alignment > 0) {
    aligned_size =
        (aligned_size + min_ubo_alignment - 1) & ~(min_ubo_alignment - 1);
  }
  return static_cast<uint32_t>(aligned_size);
}

}  // namespace Renderer