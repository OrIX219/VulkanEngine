#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Buffer.h"
#include "MaterialSystem.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

namespace Engine {
class VulkanEngine;
}

namespace Renderer {

template<typename T>
struct Handle {
  uint32_t handle;
};

class Mesh;
struct RenderObject;
struct GPUObjectData;

struct GPUIndirectObject {
  VkDrawIndexedIndirectCommand command;
};

struct DrawMesh {
  uint32_t first_vertex;
  uint32_t first_index;
  uint32_t vertex_count;
  uint32_t index_count;
  bool is_merged;

  Mesh* mesh;
};

struct SceneObject {
  Handle<DrawMesh> mesh_id;
  Handle<Material> material_id;

  uint32_t update_index;

  PerPassData<int32_t> pass_indices;

  glm::mat4 transform_matrix;

  RenderBounds bounds;
};

struct GPUInstance {
  uint32_t object_id;
  uint32_t multibatch_id;
  uint32_t first_index;
  uint32_t index_count;
  int32_t vertex_offset;
};

class RenderScene {
 public:
  struct PassMaterial {
    VkDescriptorSet material_set;
    ShaderPass* shader_pass;

    bool operator==(const PassMaterial& other) {
      return material_set == other.material_set &&
             shader_pass == other.shader_pass;
    }
  };

  struct PassObject {
    PassMaterial material;
    Handle<DrawMesh> mesh_id;
    Handle<SceneObject> original;
    int32_t built_batch;
  };

  struct RenderBatch {
    Handle<PassObject> object;
    uint64_t sort_key;

    bool operator==(const RenderBatch& other) {
      return object.handle == other.object.handle && sort_key == other.sort_key;
    }
  };

  struct IndirectBatch {
    Handle<DrawMesh> mesh_id;
    PassMaterial material;
    uint32_t first;
    uint32_t count;
    uint32_t multibatch;
  };

  struct Multibatch {
    uint32_t first;
    uint32_t count;
  };

  struct MeshPass {
    void Destroy() {
      clear_count_buffer.Destroy();
      count_buffer.Destroy();
      clear_indirect_buffer.Destroy();
      compacted_instance_buffer.Destroy();
      draw_indirect_buffer.Destroy();
      pass_objects_buffer.Destroy();
      clear_multibatches_buffer.Destroy();
      multibatches_buffer.Destroy();
    }

    std::vector<Multibatch> multibatches;
    std::vector<IndirectBatch> indirect_batches;
    std::vector<Handle<SceneObject>> unbatches_objects;
    std::vector<RenderBatch> batches;
    std::vector<PassObject> objects;
    std::vector<Handle<PassObject>> reusable_objects;
    std::vector<Handle<PassObject>> objects_to_delete;

    Buffer<true> clear_count_buffer;
    Buffer<false> count_buffer;
    Buffer<false> compacted_instance_buffer;
    Buffer<false> pass_objects_buffer;
    Buffer<true> clear_multibatches_buffer;
    Buffer<false> multibatches_buffer;

    Buffer<true> clear_indirect_buffer;
    Buffer<false> draw_indirect_buffer;

    PassObject* Get(Handle<PassObject> handle);

    MeshPassType type;

    bool needs_indirect_refresh = true;
    bool needs_instance_refresh = true;
  };

  void Init();
  void Destroy();

  Handle<SceneObject> RegisterObject(RenderObject* object);

  void RegisterObjectBatch(RenderObject* first, uint32_t count);

  void UpdateTransform(Handle<SceneObject> object_id,
                       const glm::mat4& transform);
  void UpdateObject(Handle<SceneObject> object_id);

  void FillObjectData(GPUObjectData* data);
  void FillIndirectArray(GPUIndirectObject* data, MeshPass& pass);
  void FillInstanceArray(GPUInstance* data, MeshPass& pass);
  void FillMultibatchesArray(Multibatch* data, MeshPass& pass);
  void ClearCountArray(MeshPass& pass);

  void WriteObject(GPUObjectData* target, Handle<SceneObject> object_id);

  void ClearDirtyObjects();

  void BuildBatches();
  void BuildIndirectBatches(MeshPass* pass,
                            std::vector<IndirectBatch>& out_batches,
                            std::vector<RenderBatch>& in_batches);

  void MergeMeshes(Engine::VulkanEngine* engine);

  void RefreshPass(MeshPass* pass);

  SceneObject* GetObject(Handle<SceneObject> object_id);
  DrawMesh* GetMesh(Handle<DrawMesh> mesh_id);
  Material* GetMaterial(Handle<Material> material_id);

  MeshPass forward_pass;
  MeshPass transparent_pass;
  // Should rework this. It duplicates buffers
  MeshPass directional_shadow_pass;
  MeshPass point_shadow_pass;
  
  std::vector<SceneObject> renderables;
  
  std::vector<Handle<SceneObject>> dirty_objects;

  Buffer<false> object_data_buffer;
  
  VertexBuffer merged_vertex_buffer;
  IndexBuffer merged_index_buffer;
private:
  MeshPass* GetMeshPass(MeshPassType type);
  Handle<Material> GetMaterialHandle(Material* material);
  Handle<DrawMesh> GetMeshHandle(Mesh* mesh);

  std::vector<DrawMesh> meshes_;
  std::vector<Material*> materials_;

  std::unordered_map<Material*, Handle<Material>> material_handles_;
  std::unordered_map<Mesh*, Handle<DrawMesh>> mesh_handles_;
};

}