#include "Mesh.h"

#include "MeshAsset.h"

namespace Renderer {

Mesh::Mesh() {}

Mesh::Mesh(VmaAllocator allocator, CommandBuffer command_buffer,
           const std::vector<Vertex>& vertices) {
  Create(allocator, command_buffer, vertices);
}

Mesh::~Mesh() {}

void Mesh::Create(VmaAllocator allocator, CommandBuffer command_buffer,
                  const std::vector<Vertex>& vertices) {
  vertex_buffer_.Create(allocator, sizeof(vertices.at(0)) * vertices.size());
  vertex_buffer_.SetData(command_buffer, vertices);
}

void Mesh::Create(VmaAllocator allocator, CommandBuffer command_buffer,
                  const std::vector<Vertex>& vertices,
                  const std::vector<uint32_t>& indices) {
  vertex_buffer_.Create(allocator, sizeof(vertices.at(0)) * vertices.size());
  vertex_buffer_.SetData(command_buffer, vertices);
  index_buffer_.Create(allocator, sizeof(indices.at(0)) * indices.size());
  index_buffer_.SetData(command_buffer, indices);
}

void Mesh::Destroy() {
  vertex_buffer_.Destroy();
  index_buffer_.Destroy();
}

uint32_t Mesh::GetVerticesCount() const {
  return vertex_buffer_.GetVerticesCount();
}

uint32_t Mesh::GetIndicesCount() const {
  return index_buffer_.GetIndicesCount();
}

bool Mesh::LoadFromAsset(VmaAllocator allocator, CommandBuffer command_buffer,
                         const char* path) {
  Assets::AssetFile file;
  bool loaded = Assets::LoadBinaryFile(path, file);

  if (!loaded) {
    std::cerr << "Error when loading mesh\n";
    return false;
  }

  Assets::MeshInfo mesh_info = Assets::ReadMeshInfo(file);

  std::vector<Renderer::Vertex> vertices;
  std::vector<uint32_t> indices;

  size_t size = mesh_info.vertex_buffer_size;
  if (mesh_info.vertex_format == Assets::VertexFormat::PNCV_F32)
    size /= sizeof(Assets::Vertex_f32_PNCV);
  else if (mesh_info.vertex_format == Assets::VertexFormat::P32N8C8V16)
    size /= sizeof(Assets::Vertex_P32N8C8V16);
  vertices.resize(size);

  size = mesh_info.index_buffer_size / mesh_info.index_size;
  indices.resize(size);

  Assets::UnpackMesh(&mesh_info, file.binary_blob.data(),
                     file.binary_blob.size(),
                     reinterpret_cast<char*>(vertices.data()),
                     reinterpret_cast<char*>(indices.data()));

  Create(allocator, command_buffer, vertices, indices);
  return true;
}

void Mesh::BindBuffers(CommandBuffer command_buffer) {
  VkBuffer vertex_buffers[] = {vertex_buffer_.GetBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(command_buffer.GetBuffer(), 0, 1, vertex_buffers,
                         offsets);
  vkCmdBindIndexBuffer(command_buffer.GetBuffer(), index_buffer_.GetBuffer(), 0,
                       VK_INDEX_TYPE_UINT32);
}

}  // namespace Renderer