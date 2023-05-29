#pragma once

#include "VertexBuffer.h"

namespace Renderer {

class Mesh {
 public:
  Mesh();
  Mesh(VmaAllocator allocator, CommandBuffer command_buffer,
       const std::vector<Vertex>& vertices);
  ~Mesh();
  void Create(VmaAllocator allocator, CommandBuffer command_buffer,
                     const std::vector<Vertex>& vertices);
  void Destroy();

  uint32_t GetVerticesCount() const;

  bool LoadFromObj(VmaAllocator allocator, CommandBuffer command_buffer,
                   const char* path);
  bool LoadFromAsset(VmaAllocator allocator, CommandBuffer command_buffer,
                     const char* path);

  void BindBuffers(CommandBuffer command_buffer);

 private:
  VertexBuffer vertex_buffer_;
};

}