#pragma once

#include "AssetLoader.h"

namespace Assets {

struct Vertex_f32_PNCVT {
  float position[3];
  float normal[3];
  float color[3];
  float uv[2];
  float tangent[4];
};

struct Vertex_P32N8C8V16 {
  float position[3];
  uint8_t normal[3];
  uint8_t color[3];
  float uv[2];
};

enum class VertexFormat : uint32_t { Unknown = 0, PNCVT_F32, P32N8C8V16 };

struct MeshBounds {
  float origin[3];
  float radius;
  float extents[3];
};

struct MeshInfo {
  uint64_t vertex_buffer_size;
  uint64_t index_buffer_size;
  MeshBounds bounds;
  VertexFormat vertex_format;
  char index_size;
  CompressionMode compression_mode;
  std::string original_file;
};

MeshInfo ReadMeshInfo(AssetFile& file);

void UnpackMesh(MeshInfo* info, const char* src_buffer, size_t src_size,
                char* vertex_buffer, char* index_buffer);

AssetFile PackMesh(MeshInfo* info, char* vertex_data, char* index_data);

MeshBounds CalculateBounds(Vertex_f32_PNCVT* vertices, size_t count);

}