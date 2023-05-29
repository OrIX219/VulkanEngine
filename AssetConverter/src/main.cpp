#include <filesystem>
#include <fstream>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobj/tiny_obj_loader.h>

#include <json/json.hpp>

#include "MeshAsset.h"
#include "TextureAsset.h"

namespace fs = std::filesystem;

bool ConvertImage(const fs::path& input, const fs::path& output) {
  int tex_width, tex_height, tex_channels;
  stbi_uc* pixels =
      stbi_load(input.u8string().c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

  if (!pixels) {
    std::cerr << "Failed to load texture file " << input << std::endl;
    return false;
  }

  int texture_size = tex_width * tex_height * 4;

  Assets::TextureInfo tex_info;
  tex_info.texture_size = texture_size;
  tex_info.pixel_size[0] = tex_width;
  tex_info.pixel_size[1] = tex_height;
  tex_info.texture_format = Assets::TextureFormat::RGBA8;
  tex_info.original_file = input.string();

  Assets::AssetFile new_image = Assets::PackTexture(&tex_info, pixels);

  stbi_image_free(pixels);

  Assets::SaveBinaryFile(output.string().c_str(), new_image);

  return true;
}

void PackVertex(Assets::Vertex_f32_PNCV& vertex, tinyobj::real_t vx,
                tinyobj::real_t vy, tinyobj::real_t vz, tinyobj::real_t nx,
                tinyobj::real_t ny, tinyobj::real_t nz, tinyobj::real_t tx,
                tinyobj::real_t ty) {
  vertex.position[0] = vx;
  vertex.position[1] = vy;
  vertex.position[2] = vz;

  vertex.normal[0] = nx;
  vertex.normal[1] = ny;
  vertex.normal[2] = nz;

  vertex.uv[0] = tx;
  vertex.uv[1] = 1 - ty;
}

void PackVertex(Assets::Vertex_P32N8C8V16& vertex, tinyobj::real_t vx,
                tinyobj::real_t vy, tinyobj::real_t vz, tinyobj::real_t nx,
                tinyobj::real_t ny, tinyobj::real_t nz, tinyobj::real_t tx,
                tinyobj::real_t ty) {
  vertex.position[0] = vx;
  vertex.position[1] = vy;
  vertex.position[2] = vz;

  vertex.normal[0] = static_cast<uint8_t>((nx + 1.0) / 2.0 * 255);
  vertex.normal[1] = static_cast<uint8_t>((ny + 1.0) / 2.0 * 255);
  vertex.normal[2] = static_cast<uint8_t>((nz + 1.0) / 2.0 * 255);

  vertex.uv[0] = tx;
  vertex.uv[1] = 1 - ty;
}

template<typename T>
void ExtractMeshFromObj(std::vector<tinyobj::shape_t>& shapes,
                        tinyobj::attrib_t& attrib,
                        std::vector<uint32_t>& indices,
                        std::vector<T>& vertices) {
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

        T vertex;
        PackVertex(vertex, vx, vy, vz, nx, ny, nz, tx, ty);

        indices.push_back(vertices.size());
        vertices.push_back(vertex);
      }
      index_offset += fv;
    }
  }
}

bool ConvertMesh(const fs::path& input, const fs::path& output) {
  tinyobj::ObjReaderConfig reader_config;
  reader_config.mtl_search_path = "Assets/";

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(input.string().c_str(), reader_config)) {
    if (!reader.Error().empty())
      std::cerr << "[ERROR] Model loading: " << reader.Error() << std::endl;
    return false;
  }

  if (!reader.Warning().empty())
    std::cout << "[WARNING] Model loading: " << reader.Warning() << std::endl;

  tinyobj::attrib_t attrib = reader.GetAttrib();
  std::vector<tinyobj::shape_t> shapes = reader.GetShapes();
  std::vector<tinyobj::material_t> materials = reader.GetMaterials();

  using VertexFormat = Assets::Vertex_f32_PNCV;
  auto VertexFormatEnum = Assets::VertexFormat::PNCV_F32;

  std::vector<VertexFormat> vertices;
  std::vector<uint32_t> indices;

  ExtractMeshFromObj(shapes, attrib, indices, vertices);

  Assets::MeshInfo mesh_info;
  mesh_info.vertex_format = VertexFormatEnum;
  mesh_info.vertex_buffer_size = vertices.size() * sizeof(VertexFormat);
  mesh_info.index_buffer_size = indices.size() * sizeof(uint32_t);
  mesh_info.index_size = sizeof(uint32_t);
  mesh_info.original_file = input.string();

  mesh_info.bounds = Assets::CalculateBounds(vertices.data(), vertices.size());

  Assets::AssetFile new_file =
      Assets::PackMesh(&mesh_info, reinterpret_cast<char*>(vertices.data()),
                       reinterpret_cast<char*>(indices.data()));

  Assets::SaveBinaryFile(output.string().c_str(), new_file);

  return true;
}

int main(int argc, char* argv[]) {
  fs::path path{argv[1]};
  fs::path directory = path;

  std::cout << "Loading asset directory at " << directory << std::endl;

  for (auto& p : fs::directory_iterator(directory)) {
    std::cout << "File: " << p << std::endl;

    if (p.path().extension() == ".png") {
      std::cout << "Found texture" << std::endl;

      fs::path new_path = p.path();
      new_path.replace_extension(".tx");
      ConvertImage(p.path(), new_path);
    }
    if (p.path().extension() == ".obj") {
      std::cout << "Found mesh" << std::endl;

      fs::path new_path = p.path();
      new_path.replace_extension(".mesh");
      ConvertMesh(p.path(), new_path);
    }
  }

  return 0; 
}