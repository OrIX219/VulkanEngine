#pragma once

#include <glm/glm.hpp>

#include "VertexBuffer.h"
#include "IndexBuffer.h"

namespace Renderer {

struct RenderBounds {
  glm::vec3 origin;
  float radius;
  glm::vec3 extents;
  bool valid;
};

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

  const VertexBuffer& GetVertexBuffer() const;
  VertexBuffer& GetVertexBuffer();
  const IndexBuffer& GetIndexBuffer() const;
  IndexBuffer& GetIndexBuffer();

  const RenderBounds& GetBounds() const;
  RenderBounds& GetBounds();

 private:
  VertexBuffer vertex_buffer_;
  IndexBuffer index_buffer_;

  RenderBounds bounds_;
};

}