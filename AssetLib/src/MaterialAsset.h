#pragma once

#include "AssetLoader.h"
#include <unordered_map>


namespace Assets {

enum class TransparencyMode : uint8_t { kOpaque, kTransparent, kMasked };

struct MaterialInfo {
  std::string base_effect;
  std::unordered_map<std::string, std::string> textures;
  std::unordered_map<std::string, std::string> custom_properties;
  TransparencyMode transparency;
};

MaterialInfo ReadMaterialInfo(AssetFile* file);

AssetFile PackMaterial(MaterialInfo* info);

}