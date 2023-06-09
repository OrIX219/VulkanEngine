#pragma once

#include "Buffer.h"

namespace Renderer {

class IndexBuffer {
 public:
  IndexBuffer();
  IndexBuffer(VmaAllocator allocator, uint64_t size);

  void Create(VmaAllocator allocator, uint64_t size);
  void Destroy();

  VkBuffer Get();
  uint32_t GetIndicesCount() const;

  void SetData(CommandBuffer command_buffer,
               const std::vector<uint32_t>& indices);

  void CopyTo(CommandBuffer command_buffer, IndexBuffer& dst,
              VkDeviceSize offset = 0) const;

 private:
  Buffer<false> buffer_;
  Buffer<true> staging_buffer_;
};

}  // namespace Renderer