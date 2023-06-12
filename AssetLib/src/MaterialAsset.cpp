#include "MaterialAsset.h"

#include <json/single_include/nlohmann/json.hpp>

namespace Assets {

MaterialInfo ReadMaterialInfo(AssetFile* file) {
  MaterialInfo info;

  nlohmann::json material_metadata = nlohmann::json::parse(file->json);
  info.base_effect = material_metadata["base_effect"];

  for (auto& [key, value] : material_metadata["textures"].items())
    info.textures[key] = value;
  for (auto& [key, value] : material_metadata["custom_properties"].items())
    info.custom_properties[key] = value;

  info.transparency = TransparencyMode::kOpaque;

  auto iter = material_metadata.find("transparency");
  if (iter != material_metadata.end()) {
    std::string val = *iter;
    if (val == "transparent")
      info.transparency = TransparencyMode::kTransparent;
    else if (val == "masked")
      info.transparency = TransparencyMode::kMasked;
  }

  return info;
}

AssetFile PackMaterial(MaterialInfo* info) {
  nlohmann::json material_metadata;
  material_metadata["base_effect"] = info->base_effect;
  material_metadata["textures"] = info->textures;
  material_metadata["custom_properties"] = info->custom_properties;

  switch (info->transparency) {
    case TransparencyMode::kTransparent:
      material_metadata["transparency"] = "transparent";
      break;
    case TransparencyMode::kMasked:
      material_metadata["transparency"] = "masked";
      break;
  }

  AssetFile file;
  file.type[0] = 'M';
  file.type[1] = 'A';
  file.type[2] = 'T';
  file.type[3] = 'X';
  file.version = 1;

  std::string stringified = material_metadata.dump();
  file.json = stringified;

  return file;
}

}  // namespace Assets