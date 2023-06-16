#include "IndexBuffer.h"

namespace Renderer {

IndexBuffer::IndexBuffer() {}

IndexBuffer::IndexBuffer(VmaAllocator allocator, uint64_t size) {
  Create(allocator, size);
}

void IndexBuffer::Create(VmaAllocator allocator, uint64_t size) {
  buffer_.Create(
      allocator, size,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  staging_buffer_.Create(
      allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
}

void IndexBuffer::Destroy() {
  staging_buffer_.Destroy();
  buffer_.Destroy();
}

VkBuffer IndexBuffer::GetBuffer() { return buffer_.GetBuffer(); }

uint32_t IndexBuffer::GetIndicesCount() const { return indices_count_; }

void IndexBuffer::SetData(CommandBuffer command_buffer,
                          const std::vector<uint32_t>& indices) {
  indices_count_ = static_cast<uint32_t>(indices.size());
  size_t data_size = sizeof(indices.at(0)) * indices.size();

  staging_buffer_.SetData(indices.data(), data_size);

  staging_buffer_.CopyTo(command_buffer, &buffer_);
}

void IndexBuffer::CopyTo(CommandBuffer command_buffer, IndexBuffer& dst,
                         VkDeviceSize offset) const {
  buffer_.CopyTo(command_buffer, &dst.buffer_, offset);
}

}  // namespace Renderer