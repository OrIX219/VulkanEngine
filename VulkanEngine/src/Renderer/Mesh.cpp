#include "Mesh.h"

#include "MeshAsset.h"
#include "Logger.h"

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
    LOG_ERROR("Error when loading mesh");
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

  bounds_.extents.x = mesh_info.bounds.extents[0];
  bounds_.extents.y = mesh_info.bounds.extents[1];
  bounds_.extents.z = mesh_info.bounds.extents[2];

  bounds_.origin.x = mesh_info.bounds.origin[0];
  bounds_.origin.y = mesh_info.bounds.origin[1];
  bounds_.origin.z = mesh_info.bounds.origin[2];

  bounds_.radius = mesh_info.bounds.radius;
  bounds_.valid = true;

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

const VertexBuffer& Mesh::GetVertexBuffer() const { return vertex_buffer_; }

VertexBuffer& Mesh::GetVertexBuffer() { return vertex_buffer_; }

const IndexBuffer& Mesh::GetIndexBuffer() const {
  return index_buffer_; }

IndexBuffer& Mesh::GetIndexBuffer() { return index_buffer_; }

const RenderBounds& Mesh::GetBounds() const { return bounds_; }

RenderBounds& Mesh::GetBounds() { return bounds_; }

}  // namespace Renderer