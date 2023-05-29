#pragma once

#include "AssetLoader.h"

namespace Assets {

  enum class TextureFormat : uint32_t { Unknown = 0, RGBA8 };

  struct TextureInfo {
    uint64_t texture_size;
    TextureFormat texture_format;
    CompressionMode compression_mode;
    uint32_t pixel_size[3];
    std::string original_file;
  };

  TextureInfo ReadTextureInfo(AssetFile& file);

  void UnpackTexture(TextureInfo* info, const char* src_buffer, size_t src_size,
                     char* destination);

  AssetFile PackTexture(TextureInfo* info, void* pixel_data);

}