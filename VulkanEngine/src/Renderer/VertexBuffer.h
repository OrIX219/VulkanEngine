#pragma once

#include <vma\include\vk_mem_alloc.h>

#include "Buffer.h"
#include "Vertex.h"

namespace Renderer {

class VertexBuffer {
 public:
  VertexBuffer();
  VertexBuffer(VmaAllocator allocator, uint64_t size);

  void Create(VmaAllocator allocator, uint64_t size);
  void Destroy();

  VkBuffer Get();
  uint32_t GetVerticesCount() const;

  void SetData(CommandBuffer command_buffer,
               const std::vector<Vertex>& vertices);

  void CopyTo(CommandBuffer command_buffer, VertexBuffer& dst,
              VkDeviceSize offset = 0) const;

 private:
  Buffer<false> buffer_;
  Buffer<true> staging_buffer_;
};

}