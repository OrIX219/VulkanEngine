#include "VertexBuffer.h"

namespace Renderer {

VertexBuffer::VertexBuffer() {}

VertexBuffer::VertexBuffer(VmaAllocator allocator, uint64_t size) {
  Create(allocator, size);
}

void VertexBuffer::Create(VmaAllocator allocator, uint64_t size) {
  buffer_.Create(allocator, size,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  staging_buffer_.Create(
      allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
}

void VertexBuffer::Destroy() {
  staging_buffer_.Destroy();
  buffer_.Destroy();
}

VkBuffer VertexBuffer::Get() { return buffer_.Get(); }

uint32_t VertexBuffer::GetVerticesCount() const {
  return static_cast<uint32_t>(buffer_.GetSize() / sizeof(Vertex));
}

void VertexBuffer::SetData(CommandBuffer command_buffer,
                           const std::vector<Vertex>& vertices) {
  size_t data_size = sizeof(vertices.at(0)) * vertices.size();

  staging_buffer_.SetData(vertices.data(), data_size);

  staging_buffer_.CopyTo(command_buffer, buffer_);
}

void VertexBuffer::CopyTo(CommandBuffer command_buffer, VertexBuffer& dst,
                          VkDeviceSize offset) const {
  buffer_.CopyTo(command_buffer, dst.buffer_, offset);
}

}  // namespace Renderer