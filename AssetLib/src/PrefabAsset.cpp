#include "PrefabAsset.h"

#include <json/json.hpp>

namespace Assets {

PrefabInfo ReadPrefabInfo(AssetFile* file) {
  PrefabInfo info;
  nlohmann::json prefab_metadata = nlohmann::json::parse(file->json);

  for (auto& [key, value] : prefab_metadata["node_matrices"].items())
    info.node_matrices[value[0]] = value[1];

  for (auto& [key, value] : prefab_metadata["node_names"].items())
    info.node_names[value[0]] = value[1];

  for (auto& [key, value] : prefab_metadata["node_parents"].items())
    info.node_parents[value[0]] = value[1];

  std::unordered_map<uint64_t, nlohmann::json> nodes =
      prefab_metadata["node_meshes"];
  for (auto& [key, value] : nodes) {
    PrefabInfo::NodeMesh node;
    node.mesh_path = value["mesh_path"];
    node.material_path = value["material_path"];
    info.node_meshes[key] = node;
  }

  size_t matrices_count = file->binary_blob.size() / (sizeof(float) * 16);
  info.matrices.resize(matrices_count);
  memcpy(info.matrices.data(), file->binary_blob.data(),
         file->binary_blob.size());

  return info;
}

AssetFile PackPrefab(PrefabInfo* info) {
  nlohmann::json prefab_metadata;
  prefab_metadata["node_matrices"] = info->node_matrices;
  prefab_metadata["node_names"] = info->node_names;
  prefab_metadata["node_parents"] = info->node_parents;

  std::unordered_map<uint64_t, nlohmann::json> nodes;
  for (auto& [key, value] : info->node_meshes) {
    nlohmann::json node;
    node["mesh_path"] = value.mesh_path;
    node["material_path"] = value.material_path;
    nodes[key] = node;
  }
  prefab_metadata["node_meshes"] = nodes;

  AssetFile file;
  file.type[0] = 'P';
  file.type[1] = 'R';
  file.type[2] = 'F';
  file.type[3] = 'B';
  file.version = 1;

  size_t blob_size = info->matrices.size() * sizeof(float) * 16;
  file.binary_blob.resize(blob_size);
  memcpy(file.binary_blob.data(), info->matrices.data(), blob_size);

  std::string stringified = prefab_metadata.dump();
  file.json = stringified;

  return file;
}

}  // namespace Assets