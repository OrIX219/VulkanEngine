#include "AssetLoader.h"

#include <fstream>

namespace Assets {

bool SaveBinaryFile(const char* path, const AssetFile& file) {
  std::ofstream out_file;
  out_file.open(path, std::ios::binary | std::ios::out);

  out_file.write(file.type, 4);

  uint32_t version = file.version;
  out_file.write(reinterpret_cast<const char*>(&version), sizeof(uint32_t));

  uint32_t length = file.json.size();
  out_file.write(reinterpret_cast<const char*>(&length), sizeof(uint32_t));

  uint32_t blob_length = file.binary_blob.size();
  out_file.write(reinterpret_cast<const char*>(&blob_length), sizeof(uint32_t));

  out_file.write(file.json.data(), length);
  out_file.write(file.binary_blob.data(), blob_length);

  return true;
}

bool LoadBinaryFile(const char* path, AssetFile& output_file) {
  std::ifstream in_file;
  in_file.open(path, std::ios::binary);

  if (!in_file.is_open()) return false;

  in_file.seekg(0);

  in_file.read(output_file.type, 4);

  in_file.read(reinterpret_cast<char*>(&output_file.version), sizeof(uint32_t));

  uint32_t length = 0;
  in_file.read(reinterpret_cast<char*>(&length), sizeof(uint32_t));

  uint32_t blob_length = 0;
  in_file.read(reinterpret_cast<char*>(&blob_length), sizeof(uint32_t));

  output_file.json.resize(length);
  in_file.read(output_file.json.data(), length);

  output_file.binary_blob.resize(blob_length);
  in_file.read(output_file.binary_blob.data(), blob_length);

  return true;
}

CompressionMode ParseCompression(const char* compression) {
  if (strcmp(compression, "LZ4") == 0)
    return CompressionMode::LZ4;
  else
    return CompressionMode::None;
}

}  // namespace Assets