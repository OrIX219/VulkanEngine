#pragma once

#include <string>
#include <vector>

namespace Assets {
  
  struct AssetFile {
    char type[4];
    int version;
    std::string json;
    std::vector<char> binary_blob;
  };

  enum class CompressionMode : uint32_t {None = 0, LZ4};

  bool SaveBinaryFile(const char* path, const AssetFile& file);
  bool LoadBinaryFile(const char* path, AssetFile& output_file);

  CompressionMode ParseCompression(const char* compression);

}