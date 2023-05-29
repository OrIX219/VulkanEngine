#include "TextureAsset.h"

#include <json/json.hpp>
#include <lz4/lz4.h>

namespace Assets {

TextureFormat ParseFormat(const char* format) {
  if (strcmp(format, "RGBA8") == 0)
    return TextureFormat::RGBA8;
  else
    return TextureFormat::Unknown;
}

TextureInfo ReadTextureInfo(AssetFile& file) {
  TextureInfo info;
  nlohmann::json texture_metadata = nlohmann::json::parse(file.json);

  std::string format_string = texture_metadata["format"];
  info.texture_format = ParseFormat(format_string.c_str());

  std::string compression_string = texture_metadata["compression"];
  info.compression_mode = ParseCompression(compression_string.c_str());

  info.pixel_size[0] = texture_metadata["width"];
  info.pixel_size[1] = texture_metadata["height"];
  info.texture_size = texture_metadata["buffer_size"];
  info.original_file = texture_metadata["original_file"];

  return info;
}

void UnpackTexture(TextureInfo* info, const char* src_buffer, size_t src_size,
                   char* destination) {
  if (info->compression_mode == CompressionMode::LZ4) {
    LZ4_decompress_safe(src_buffer, destination, src_size, info->texture_size);
  } else {
    memcpy(destination, src_buffer, src_size);
  }
}

AssetFile PackTexture(TextureInfo* info, void* pixel_data) {
  nlohmann::json texture_metadata;
  texture_metadata["format"] = "RGBA8";
  texture_metadata["width"] = info->pixel_size[0];
  texture_metadata["height"] = info->pixel_size[1];
  texture_metadata["buffer_size"] = info->texture_size;
  texture_metadata["original_file"] = info->original_file;

  AssetFile file;
  file.type[0] = 'T';
  file.type[1] = 'E';
  file.type[2] = 'X';
  file.type[3] = 'I';
  file.version = 1;

  int compress_staging = LZ4_compressBound(info->texture_size);
  file.binary_blob.resize(compress_staging);

  int compressed_size = LZ4_compress_default(
      static_cast<const char*>(pixel_data), file.binary_blob.data(),
      info->texture_size, compress_staging);
  file.binary_blob.resize(compressed_size);

  texture_metadata["compression"] = "LZ4";

  std::string stringified = texture_metadata.dump();
  file.json = stringified;

  return file;
}

}  // namespace Assets