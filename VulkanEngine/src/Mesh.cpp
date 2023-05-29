#include "Mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobj/tiny_obj_loader.h>

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

void Mesh::Destroy() { vertex_buffer_.Destroy(); }

uint32_t Mesh::GetVerticesCount() const {
  return vertex_buffer_.GetVerticesCount();
}

bool Mesh::LoadFromObj(VmaAllocator allocator, CommandBuffer command_buffer,
                       const char* path) {
  tinyobj::ObjReaderConfig reader_config;
  reader_config.mtl_search_path = "Assets/";

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(path, reader_config)) {
    if (!reader.Error().empty())
      std::cerr << "[ERROR] Model loading: " << reader.Error() << std::endl;
    return false;
  }

  if (!reader.Warning().empty())
    std::cout << "[WARNING] Model loading: " << reader.Warning() << std::endl;

  const tinyobj::attrib_t& attrib = reader.GetAttrib();
  const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
  const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();

  std::vector<Vertex> vertices;

  for (size_t s = 0; s < shapes.size(); ++s) {
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); ++f) {
      size_t fv = static_cast<size_t>(shapes[s].mesh.num_face_vertices[f]);
      for (size_t v = 0; v < fv; ++v) {
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
        tinyobj::real_t vx =
            attrib.vertices[3 * static_cast<size_t>(idx.vertex_index) + 0];
        tinyobj::real_t vy =
            attrib.vertices[3 * static_cast<size_t>(idx.vertex_index) + 1];
        tinyobj::real_t vz =
            attrib.vertices[3 * static_cast<size_t>(idx.vertex_index) + 2];

        tinyobj::real_t nx, ny, nz, tx, ty;
        if (idx.normal_index >= 0) {
          nx = attrib.normals[3 * static_cast<size_t>(idx.normal_index) + 0];
          ny = attrib.normals[3 * static_cast<size_t>(idx.normal_index) + 1];
          nz = attrib.normals[3 * static_cast<size_t>(idx.normal_index) + 2];
        }

        if (idx.texcoord_index >= 0) {
          tx =
              attrib.texcoords[2 * static_cast<size_t>(idx.texcoord_index) + 0];
          ty =
              attrib.texcoords[2 * static_cast<size_t>(idx.texcoord_index) + 1];
        }

        Vertex vertex;
        vertex.pos = {vx, vy, vz};
        vertex.normal = {nx, ny, nz};
        vertex.color = vertex.normal;
        vertex.texture_coords = {tx, 1 - ty};

        vertices.push_back(vertex);
      }
      index_offset += fv;
    }
  }
  
  Create(allocator, command_buffer, vertices);
  return true;
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
  size_t size = mesh_info.vertex_buffer_size;
  if (mesh_info.vertex_format == Assets::VertexFormat::PNCV_F32)
    size /= sizeof(Assets::Vertex_f32_PNCV);
  else if (mesh_info.vertex_format == Assets::VertexFormat::P32N8C8V16)
    size /= sizeof(Assets::Vertex_P32N8C8V16);

  vertices.resize(size);

  Assets::UnpackMesh(&mesh_info, file.binary_blob.data(),
                     file.binary_blob.size(),
                     reinterpret_cast<char*>(vertices.data()), nullptr);

  Create(allocator, command_buffer, vertices);
  return true;
}

void Mesh::BindBuffers(CommandBuffer command_buffer) {
  VkBuffer vertex_buffers[] = {vertex_buffer_.GetBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(command_buffer.GetBuffer(), 0, 1, vertex_buffers,
                         offsets);
}

}  // namespace Renderer