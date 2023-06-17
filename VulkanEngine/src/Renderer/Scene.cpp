#include "Scene.h"

#include <future>

#include "RenderObject.h"
#include "VulkanEngine.h"
#include "Logger.h"

namespace Renderer {

RenderScene::PassObject* RenderScene::MeshPass::Get(Handle<PassObject> handle) {
  return &objects[handle.handle];
}

void RenderScene::Init() {
  forward_pass.type = MeshPassType::kForward;
  transparent_pass.type = MeshPassType::kTransparency;
  shadow_pass.type = MeshPassType::kDirectionalShadow;
}

void RenderScene::Destroy() {
  merged_index_buffer.Destroy();
  merged_vertex_buffer.Destroy();
  object_data_buffer.Destroy();

  forward_pass.Destroy();
  transparent_pass.Destroy();
  shadow_pass.Destroy();
}

Handle<SceneObject> RenderScene::RegisterObject(RenderObject* object) {
  SceneObject new_object;
  new_object.transform_matrix = object->model_mat;
  new_object.material_id = GetMaterialHandle(object->material);
  new_object.mesh_id = GetMeshHandle(object->mesh);
  new_object.update_index = -1;
  new_object.pass_indices.Clear(-1);
  Handle<SceneObject> handle;
  handle.handle = static_cast<uint32_t>(renderables.size());

  renderables.push_back(new_object);

  if (object->draw_forward_pass) {
    if (object->material->original->pass_shaders[MeshPassType::kTransparency])
      transparent_pass.unbatches_objects.push_back(handle);
    if (object->material->original->pass_shaders[MeshPassType::kForward])
      forward_pass.unbatches_objects.push_back(handle);
  }
  if (object->draw_shadow_pass) {
    if (object->material->original
            ->pass_shaders[MeshPassType::kDirectionalShadow])
      shadow_pass.unbatches_objects.push_back(handle);
  }

  UpdateObject(handle);
  return handle;
}

void RenderScene::RegisterObjectBatch(RenderObject* first, uint32_t count) {
  renderables.reserve(count);
  for (size_t i = 0; i < count; ++i) RegisterObject(&first[i]);
}

void RenderScene::UpdateTransform(Handle<SceneObject> object_id,
                                  const glm::mat4& transform) {
  GetObject(object_id)->transform_matrix = transform;
  UpdateObject(object_id);
}

void RenderScene::UpdateObject(Handle<SceneObject> object_id) {
  PerPassData<int32_t>& pass_indices = GetObject(object_id)->pass_indices;
  
  if (pass_indices[MeshPassType::kForward] != -1) {
    Handle<PassObject> obj;
    obj.handle = pass_indices[MeshPassType::kForward];

    forward_pass.objects_to_delete.push_back(obj);
    forward_pass.unbatches_objects.push_back(object_id);

    pass_indices[MeshPassType::kForward] = -1;
  }

  if (pass_indices[MeshPassType::kTransparency] != -1) {
    Handle<PassObject> obj;
    obj.handle = pass_indices[MeshPassType::kTransparency];

    transparent_pass.objects_to_delete.push_back(obj);
    transparent_pass.unbatches_objects.push_back(object_id);

    pass_indices[MeshPassType::kTransparency] = -1;
  }

  if (pass_indices[MeshPassType::kDirectionalShadow] != -1) {
    Handle<PassObject> obj;
    obj.handle = pass_indices[MeshPassType::kDirectionalShadow];

    shadow_pass.objects_to_delete.push_back(obj);
    shadow_pass.unbatches_objects.push_back(object_id);

    pass_indices[MeshPassType::kDirectionalShadow] = -1;
  }

  if (GetObject(object_id)->update_index == static_cast<uint32_t>(-1)) {
    GetObject(object_id)->update_index =
        static_cast<uint32_t>(dirty_objects.size());
    dirty_objects.push_back(object_id);
  }
}

void RenderScene::FillObjectData(GPUObjectData* data) {
  for (uint32_t i = 0; i < renderables.size(); ++i) {
    Handle<SceneObject> handle;
    handle.handle = i;
    WriteObject(data + i, handle);
  }
}

void RenderScene::FillIndirectArray(GPUIndirectObject* data, MeshPass& pass) {
  for (size_t i = 0; i < pass.indirect_batches.size(); ++i) {
    const IndirectBatch& batch = pass.indirect_batches[i];
    data[i].command.firstInstance = batch.first;
    data[i].command.instanceCount = 0; // Gets incremented in compute shader
    data[i].command.firstIndex = GetMesh(batch.mesh_id)->first_index;
    data[i].command.vertexOffset = GetMesh(batch.mesh_id)->first_vertex;
    data[i].command.indexCount = GetMesh(batch.mesh_id)->index_count;
    data[i].object_id = 0;
    data[i].batch_id = static_cast<uint32_t>(i);
  }
}

void RenderScene::FillInstanceArray(GPUInstance* data, MeshPass& pass) {
  size_t data_idx = 0;
  for (size_t i = 0; i < pass.indirect_batches.size(); ++i) {
    const IndirectBatch& batch = pass.indirect_batches[i];
    for (size_t j = 0; j < batch.count; ++j) {
      data[data_idx].object_id =
          pass.Get(pass.batches[j + batch.first].object)->original.handle;
      data[data_idx].batch_id = static_cast<uint32_t>(i);
      ++data_idx;
    }
  }
}

void RenderScene::ClearCountArray(MeshPass& pass) {
  memset(pass.clear_count_buffer.GetMappedMemory(), 0,
         pass.clear_count_buffer.GetSize());
}

void RenderScene::WriteObject(GPUObjectData* target,
                              Handle<SceneObject> object_id) {
  SceneObject* renderable = GetObject(object_id);
  GPUObjectData object;

  object.model_matrix = renderable->transform_matrix;

  memcpy(target, &object, sizeof(GPUObjectData));
}

void RenderScene::ClearDirtyObjects() {
  for (Handle<SceneObject> obj : dirty_objects)
    GetObject(obj)->update_index = static_cast<uint32_t>(-1);
  dirty_objects.clear();
}

void RenderScene::BuildBatches() {
  auto forward =
      std::async(std::launch::async, [&]() { RefreshPass(&forward_pass); });
  auto transparent =
      std::async(std::launch::async, [&]() { RefreshPass(&transparent_pass); });
  auto shadow =
      std::async(std::launch::async, [&]() { RefreshPass(&shadow_pass); });

  forward.get();
  transparent.get();
  shadow.get();
}

void RenderScene::BuildIndirectBatches(MeshPass* pass,
                                       std::vector<IndirectBatch>& out_batches,
                                       std::vector<RenderBatch>& in_batches) {
  if (in_batches.size() == 0) return;

  IndirectBatch new_batch{};
  new_batch.material = pass->Get(in_batches[0].object)->material;
  new_batch.mesh_id = pass->Get(in_batches[0].object)->mesh_id;

  out_batches.push_back(new_batch);

  IndirectBatch* back = &pass->indirect_batches.back();

  for (size_t i = 0; i < in_batches.size(); ++i) {
    PassObject* obj = pass->Get(in_batches[i].object);

    bool same_mesh = obj->mesh_id.handle == back->mesh_id.handle;
    bool same_material = obj->material == back->material;
    
    if (!same_material || !same_mesh) {
      new_batch.material = obj->material;
      if (new_batch.material == back->material) same_material = true;
    }

    if (same_mesh && same_material) {
      ++back->count;
    } else {
      new_batch.first = static_cast<uint32_t>(i);
      new_batch.count = 1;
      new_batch.mesh_id = obj->mesh_id;
      out_batches.push_back(new_batch);
      back = &out_batches.back();
    }
  }
}

void RenderScene::MergeMeshes(Engine::VulkanEngine* engine) {
  size_t total_vertices = 0;
  size_t total_indices = 0;

  for (DrawMesh& mesh : meshes_) {
    mesh.first_vertex = static_cast<uint32_t>(total_vertices);
    mesh.first_index = static_cast<uint32_t>(total_indices);

    total_vertices += mesh.vertex_count;
    total_indices += mesh.index_count;

    mesh.is_merged = true;
  }
  
  merged_vertex_buffer.Create(engine->GetAllocator(),
                              total_vertices * sizeof(Vertex));
  merged_index_buffer.Create(engine->GetAllocator(),
                             total_indices * sizeof(uint32_t));

  CommandBuffer command_buffer = engine->upload_pool_.GetBuffer();
  command_buffer.Begin(true);

  for (DrawMesh& mesh : meshes_) {
    mesh.mesh->GetVertexBuffer().CopyTo(command_buffer, merged_vertex_buffer,
                                        mesh.first_vertex * sizeof(Vertex));
    mesh.mesh->GetIndexBuffer().CopyTo(command_buffer, merged_index_buffer,
                                       mesh.first_index * sizeof(uint32_t));
  }

  command_buffer.End();
  command_buffer.Submit();
}

void RenderScene::RefreshPass(MeshPass* pass) {
  pass->needs_indirect_refresh = true;
  pass->needs_instance_refresh = true;

  std::vector<uint32_t> new_objects;
  if (pass->objects_to_delete.size() > 0) {
    std::vector<RenderBatch> deletion_batches;
    deletion_batches.reserve(pass->objects_to_delete.size());

    for (Handle<PassObject> obj : pass->objects_to_delete) {
      pass->reusable_objects.push_back(obj);
      RenderBatch new_batch;

      PassObject object = pass->objects[obj.handle];
      new_batch.object = obj;

      uint64_t pipeline_hash = std::hash<uint64_t>()(
          uint64_t(object.material.shader_pass->pipeline));
      int64_t set_hash =
          std::hash<uint64_t>()(uint64_t(object.material.material_set));

      uint64_t material_hash = pipeline_hash ^ set_hash;
      uint64_t mesh_hash =
          material_hash ^ static_cast<uint64_t>(object.mesh_id.handle);

      new_batch.sort_key = mesh_hash;

      pass->objects[obj.handle].material.shader_pass = nullptr;
      pass->objects[obj.handle].mesh_id.handle = -1;
      pass->objects[obj.handle].original.handle = -1;

      deletion_batches.push_back(new_batch);
    }

    pass->objects_to_delete.clear();

    std::sort(deletion_batches.begin(), deletion_batches.end(),
              [](const RenderBatch& lhs, const RenderBatch& rhs) {
                return (lhs.sort_key == rhs.sort_key
                            ? lhs.object.handle < rhs.object.handle
                            : lhs.sort_key < rhs.sort_key);
              });

    std::vector<RenderBatch> new_batches;
    new_batches.reserve(pass->batches.size());
    std::set_difference(pass->batches.begin(), pass->batches.end(),
                        deletion_batches.begin(), deletion_batches.end(),
                        std::back_inserter(new_batches),
                        [](const RenderBatch& lhs, const RenderBatch& rhs) {
                          return (lhs.sort_key == rhs.sort_key
                                      ? lhs.object.handle < rhs.object.handle
                                      : lhs.sort_key < rhs.sort_key);
                        });
    pass->batches = std::move(new_batches);
  }

  new_objects.reserve(pass->unbatches_objects.size());
  for (Handle<SceneObject> obj : pass->unbatches_objects) {
    PassObject new_object;
    new_object.original = obj;
    new_object.mesh_id = GetObject(obj)->mesh_id;

    Material* material = GetMaterial(GetObject(obj)->material_id);
    new_object.material.material_set = material->pass_sets[pass->type];
    new_object.material.shader_pass =
        material->original->pass_shaders[pass->type];

    uint32_t handle = -1;

    if (pass->reusable_objects.size() > 0) {
      handle = pass->reusable_objects.back().handle;
      pass->reusable_objects.pop_back();
      pass->objects[handle] = new_object;
    } else {
      handle = static_cast<uint32_t>(pass->objects.size());
      pass->objects.push_back(new_object);
    }

    new_objects.push_back(handle);
    GetObject(obj)->pass_indices[pass->type] = static_cast<int32_t>(handle);
  }
  pass->unbatches_objects.clear();

  std::vector<RenderBatch> new_batches;
  new_batches.reserve(new_objects.size());
  for (uint32_t i : new_objects) {
    RenderBatch new_batch;
    PassObject object = pass->objects[i];
    new_batch.object.handle = i;

    uint64_t pipeline_hash =
        std::hash<uint64_t>()(uint64_t(object.material.shader_pass->pipeline));
    int64_t set_hash =
        std::hash<uint64_t>()(uint64_t(object.material.material_set));

    uint64_t material_hash = pipeline_hash ^ set_hash;
    uint64_t mesh_hash =
        material_hash ^ static_cast<uint64_t>(object.mesh_id.handle);

    new_batch.sort_key = mesh_hash;

    new_batches.push_back(new_batch);
  }

  std::sort(new_batches.begin(), new_batches.end(),
            [](const RenderBatch& lhs, const RenderBatch& rhs) {
              return (lhs.sort_key == rhs.sort_key
                          ? lhs.object.handle < rhs.object.handle
                          : lhs.sort_key < rhs.sort_key);
            });

  if (pass->batches.size() > 0 && new_batches.size() > 0) {
    size_t index = pass->batches.size();
    pass->batches.reserve(pass->batches.size() + new_batches.size());

    for (RenderBatch batch : new_batches) pass->batches.push_back(batch);

    RenderBatch* begin = pass->batches.data();
    RenderBatch* mid = begin + index;
    RenderBatch* end = begin + pass->batches.size();

    std::inplace_merge(begin, mid, end,
                       [](const RenderBatch& lhs, const RenderBatch& rhs) {
                         return (lhs.sort_key == rhs.sort_key
                                     ? lhs.object.handle < rhs.object.handle
                                     : lhs.sort_key < rhs.sort_key);
                       });
  } else if (pass->batches.size() == 0) {
    pass->batches = std::move(new_batches);
  }

  pass->indirect_batches.clear();

  BuildIndirectBatches(pass, pass->indirect_batches, pass->batches);

  Multibatch new_batch;
  pass->multibatches.clear();

  new_batch.first = 0;
  new_batch.count = 1;

  for (size_t i = 1; i < pass->indirect_batches.size(); ++i) {
    IndirectBatch* join_batch = &pass->indirect_batches[new_batch.first];
    IndirectBatch* batch = &pass->indirect_batches[i];

    bool compatible_mesh = GetMesh(join_batch->mesh_id)->is_merged;
    bool same_material = false;
    if (compatible_mesh &&
        join_batch->material.material_set == batch->material.material_set &&
        join_batch->material.shader_pass == batch->material.shader_pass)
      same_material = true;

    if (compatible_mesh && same_material) {
      ++new_batch.count;
    } else {
      pass->multibatches.push_back(new_batch);
      new_batch.first = static_cast<uint32_t>(i);
      new_batch.count = 1;
    }
  }
  pass->multibatches.push_back(new_batch);
}

SceneObject* RenderScene::GetObject(Handle<SceneObject> object_id) {
  return &renderables[object_id.handle];
}

DrawMesh* RenderScene::GetMesh(Handle<DrawMesh> mesh_id) {
  return &meshes_[mesh_id.handle];
}

Material* RenderScene::GetMaterial(Handle<Material> material_id) {
  return materials_[material_id.handle];
}

RenderScene::MeshPass* RenderScene::GetMeshPass(MeshPassType type) {
  switch (type) {
    case MeshPassType::kForward:
      return &forward_pass;
    case MeshPassType::kTransparency:
      return &transparent_pass;
    case MeshPassType::kDirectionalShadow:
      return &shadow_pass;
  }
  return nullptr;
}

Handle<Material> RenderScene::GetMaterialHandle(Material* material) {
  auto iter = material_handles_.find(material);
  if (iter != material_handles_.end()) 
    return iter->second;

  Handle<Material> handle;
  uint32_t index = static_cast<uint32_t>(materials_.size());
  materials_.push_back(material);
  handle.handle = index;
  material_handles_[material] = handle;
  return handle;
}

Handle<DrawMesh> RenderScene::GetMeshHandle(Mesh* mesh) {
  auto iter = mesh_handles_.find(mesh);
  if (iter != mesh_handles_.end()) return iter->second;

  Handle<DrawMesh> handle;
  uint32_t index = static_cast<uint32_t>(meshes_.size());
  DrawMesh new_mesh;
  new_mesh.mesh = mesh;
  new_mesh.is_merged = false;
  new_mesh.first_index = 0;
  new_mesh.first_vertex = 0;
  new_mesh.index_count = static_cast<uint32_t>(mesh->GetIndicesCount());
  new_mesh.vertex_count = static_cast<uint32_t>(mesh->GetVerticesCount());

  meshes_.push_back(new_mesh);

  handle.handle = index;

  return handle;
}

}  // namespace Renderer