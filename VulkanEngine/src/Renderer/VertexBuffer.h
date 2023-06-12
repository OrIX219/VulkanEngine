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

  VkBuffer GetBuffer();
  uint32_t GetVerticesCount() const;

  void SetData(CommandBuffer command_buffer,
               const std::vector<Vertex>& vertices);

 private:
  uint32_t vertices_count_;
  Buffer<false> buffer_;
  Buffer<true> staging_buffer_;
};

}