#pragma once

#include "VertexBuffer.h"
#include "IndexBuffer.h"

namespace Renderer {

class Mesh {
 public:
  Mesh();
  Mesh(VmaAllocator allocator, CommandBuffer command_buffer,
       const std::vector<Vertex>& vertices);
  ~Mesh();
  void Create(VmaAllocator allocator, CommandBuffer command_buffer,
              const std::vector<Vertex>& vertices);
  void Create(VmaAllocator allocator, CommandBuffer command_buffer,
              const std::vector<Vertex>& vertices,
              const std::vector<uint32_t>& indices);
  void Destroy();

  uint32_t GetVerticesCount() const;
  uint32_t GetIndicesCount() const;

  bool LoadFromAsset(VmaAllocator allocator, CommandBuffer command_buffer,
                     const char* path);

  void BindBuffers(CommandBuffer command_buffer);

 private:
  VertexBuffer vertex_buffer_;
  IndexBuffer index_buffer_;
};

}