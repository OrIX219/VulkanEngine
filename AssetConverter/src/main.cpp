#include <filesystem>
#include <fstream>
#include <iostream>

#include <json/json.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobj/tiny_obj_loader.h>

#include "AssetLoader.h"
#include "MeshAsset.h"
#include "TextureAsset.h"

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

namespace fs = std::filesystem;

struct ConverterState {
  fs::path asset_path;
  fs::path export_path;

  fs::path ConvertToExportRelative(fs::path path) const {
    return path.lexically_proximate(export_path);
  }
};

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

void UnpackGltfBuffer(tinygltf::Model& model, tinygltf::Accessor& accessor,
                      std::vector<uint8_t>& output_buffer) {
  int buffer_id = accessor.bufferView;
  size_t element_size =
      tinygltf::GetComponentSizeInBytes(accessor.componentType);

  tinygltf::BufferView& buffer_view = model.bufferViews[buffer_id];
  tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

  uint8_t* data_ptr =
      buffer.data.data() + accessor.byteOffset + buffer_view.byteOffset;

  int components = tinygltf::GetNumComponentsInType(accessor.type);

  element_size *= components;

  size_t stride = buffer_view.byteStride;
  if (stride == 0) stride = element_size;

  output_buffer.resize(accessor.count * element_size);

  for (size_t i = 0; i < accessor.count; ++i) {
    uint8_t* data_idx = data_ptr + stride * i;
    uint8_t* target_ptr = output_buffer.data() + element_size * i;
    memcpy(target_ptr, data_idx, element_size);
  }
}

void ExtractGltfVertices(tinygltf::Primitive& primitive, tinygltf::Model& model,
                         std::vector<Assets::Vertex_f32_PNCV>& vertices) {
  tinygltf::Accessor& pos_accessor =
      model.accessors[primitive.attributes["POSITION"]];

  vertices.resize(pos_accessor.count);

  std::vector<uint8_t> pos_data;
  UnpackGltfBuffer(model, pos_accessor, pos_data);

  for (size_t i = 0; i < vertices.size(); ++i) {
    if (pos_accessor.type != TINYGLTF_TYPE_VEC3) assert(false && "Unsupported position type");
    if (pos_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
      assert(false && "Unsupported position type");

    float* data = reinterpret_cast<float*>(pos_data.data());

    vertices[i].position[0] = *(data + (i * 3) + 0);
    vertices[i].position[1] = *(data + (i * 3) + 1);
    vertices[i].position[2] = *(data + (i * 3) + 2);
  }

  tinygltf::Accessor& normal_accessor =
      model.accessors[primitive.attributes["NORMAL"]];

  std::vector<uint8_t> normal_data;
  UnpackGltfBuffer(model, normal_accessor, normal_data);

  for (size_t i = 0; i < vertices.size(); ++i) {
    if (normal_accessor.type != TINYGLTF_TYPE_VEC3)
      assert(false && "Unsupported normal type");
    if (normal_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
      assert(false && "Unsupported normal type");

    float* data = reinterpret_cast<float*>(normal_data.data());

    vertices[i].normal[0] = *(data + (i * 3) + 0);
    vertices[i].normal[1] = *(data + (i * 3) + 1);
    vertices[i].normal[2] = *(data + (i * 3) + 2);

    vertices[i].color[0] = *(data + (i * 3) + 0);
    vertices[i].color[1] = *(data + (i * 3) + 1);
    vertices[i].color[2] = *(data + (i * 3) + 2);
  }

  tinygltf::Accessor& uv_accessor =
      model.accessors[primitive.attributes["TEXCOORD_0"]];

  std::vector<uint8_t> uv_data;
  UnpackGltfBuffer(model, uv_accessor, uv_data);

  for (size_t i = 0; i < vertices.size(); ++i) {
    if (uv_accessor.type != TINYGLTF_TYPE_VEC2)
      assert(false && "Unsupported UV type");
    if (uv_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
      assert(false && "Unsupported UV type");

    float* data = reinterpret_cast<float*>(uv_data.data());

    vertices[i].uv[0] = *(data + (i * 3) + 0);
    vertices[i].uv[1] = *(data + (i * 3) + 1);
  }
}

void ExtractGltfIndices(tinygltf::Primitive& primitive, tinygltf::Model& model,
                        std::vector<uint32_t>& indices) {
  tinygltf::Accessor& index_accessor = model.accessors[primitive.indices];

  int index_buffer = index_accessor.bufferView;
  int component_type = index_accessor.componentType;
  size_t index_size = tinygltf::GetComponentSizeInBytes(component_type);

  tinygltf::BufferView& index_view = model.bufferViews[index_buffer];
  int buffer_idx = index_view.buffer;

  tinygltf::Buffer& buffer = model.buffers[buffer_idx];

  uint8_t* data_ptr = buffer.data.data() + index_view.byteOffset;

  std::vector<uint8_t> indices_data;
  UnpackGltfBuffer(model, index_accessor, indices_data);

  for (size_t i = 0; i < index_accessor.count; ++i) {
    uint32_t index;
    switch (component_type) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        uint16_t* data = reinterpret_cast<uint16_t*>(indices_data.data());
        index = *(data + i);
      } break;
      case TINYGLTF_COMPONENT_TYPE_SHORT: {
        int16_t* data = reinterpret_cast<int16_t*>(indices_data.data());
        index = *(data + i);
      } break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        uint32_t* data = reinterpret_cast<uint32_t*>(indices_data.data());
        index = *(data + i);
      } break;
      case TINYGLTF_COMPONENT_TYPE_INT: {
        int32_t* data = reinterpret_cast<int32_t*>(indices_data.data());
        index = *(data + i);
      } break;
      default:
        assert(false && "Upsupported index type");
    }
    indices.push_back(index);
  }

  for (size_t i = 0; i < indices.size() / 3; ++i) {
    // Flip triangle
    std::swap(indices[i * 3 + 1], indices[i * 3 + 2]);
  }
}

std::string CalculateGltfMeshName(tinygltf::Model& model, size_t mesh_idx,
                                  size_t prim_idx) {
  char buf0[50];
  char buf1[50];
  itoa(mesh_idx, buf0, 10);
  itoa(prim_idx, buf1, 10);

  std::string mesh_name =
      "MESH_" + std::string{buf0} + "_" + model.meshes[mesh_idx].name;

  bool multiprim = model.meshes[mesh_idx].primitives.size() > 1;
  if (multiprim) mesh_name += "_PRIM_" + std::string{buf1};
  return mesh_name;
}

bool ExtractGltfMeshes(tinygltf::Model& model, const fs::path& input,
                       const fs::path& output, const ConverterState& state) {
  tinygltf::Model* glmod = &model;
  for (size_t mesh_idx = 0; mesh_idx < model.meshes.size(); ++mesh_idx) {
    tinygltf::Mesh& glmesh = model.meshes[mesh_idx];

    using VertexFormat = Assets::Vertex_f32_PNCV;
    auto VertexFormatEnum = Assets::VertexFormat::PNCV_F32;

    std::vector<VertexFormat> vertices;
    std::vector<uint32_t> indices;

    for (size_t prim_idx = 0; prim_idx < glmesh.primitives.size(); ++prim_idx) {
      vertices.clear();
      indices.clear();

      std::string mesh_name = CalculateGltfMeshName(model, mesh_idx, prim_idx);

      tinygltf::Primitive& primitive = glmesh.primitives[prim_idx];

      ExtractGltfIndices(primitive, model, indices);
      ExtractGltfVertices(primitive, model, vertices);

      Assets::MeshInfo mesh_info;
      mesh_info.vertex_format = VertexFormatEnum;
      mesh_info.vertex_buffer_size = vertices.size() * sizeof(VertexFormat);
      mesh_info.index_buffer_size = indices.size() * sizeof(uint32_t);
      mesh_info.index_size = sizeof(uint32_t);
      mesh_info.original_file = input.string();
      mesh_info.bounds =
          Assets::CalculateBounds(vertices.data(), vertices.size());

      Assets::AssetFile file =
          Assets::PackMesh(&mesh_info, reinterpret_cast<char*>(vertices.data()),
                           reinterpret_cast<char*>(indices.data()));

      fs::path mesh_path = output / (mesh_name + ".mesh");

      Assets::SaveBinaryFile(mesh_path.string().c_str(), file);
    }
  }
  return true;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "No path specified\n";
    return -1;
  }

  fs::path path{argv[1]};
  fs::path directory = path;
  fs::path export_dir = path.parent_path() / "asset_export";

  std::cout << "Loading asset directory at " << directory << std::endl;

  ConverterState state;
  state.asset_path = path;
  state.export_path = export_dir;

  for (auto& p : fs::directory_iterator(directory)) {
    std::cout << "File: " << p << std::endl;

    fs::path relative = p.path().lexically_proximate(directory);
    fs::path export_path = export_dir / relative;

    if (!fs::is_directory(export_path.parent_path()))
      fs::create_directory(export_path.parent_path());

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
    if (p.path().extension() == ".gltf") {
      std::cout << "Found mesh" << std::endl;

      tinygltf::Model model;
      tinygltf::TinyGLTF loader;
      std::string error;
      std::string warning;

      bool res = loader.LoadASCIIFromFile(&model, &error, &warning,
                                          p.path().string().c_str());

      if (!warning.empty()) std::cerr << "Warning: " << warning << "\n";
      if (!error.empty()) std::cerr << "Error: " << error << "\n";

      if (!res) {
        std::cerr << "Failed to parse glTF\n";
        continue;
      }

      fs::path folder =
          export_path.parent_path() / (p.path().stem().string() + "_GLTF");
      fs::create_directory(folder);

      ExtractGltfMeshes(model, p.path(), folder, state);
    }
  }

  return 0; 
}