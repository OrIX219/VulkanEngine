#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <vector>
#include <string>

#include "Pipeline.h"
#include "Mesh.h"
#include "RenderPass.h"

#include "MaterialAsset.h"

namespace Engine {
class VulkanEngine;
}

namespace Renderer {

class ShaderEffect;
struct ShaderPass {
  ShaderEffect* effect{nullptr};
  VkPipeline pipeline{VK_NULL_HANDLE};
  VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
};

struct SampledTexture {
  VkSampler sampler;
  VkImageView view;
};

enum class MeshPassType { kNone, kForward, kTransparency, kDirectionalShadow };

template<typename T>
class PerPassData {
 public:
  T& operator[](MeshPassType pass) {
    switch (pass) {
      case MeshPassType::kForward:
        return data[0];
      case MeshPassType::kTransparency:
        return data[1];
      case MeshPassType::kDirectionalShadow:
        return data[2];
    }
    return data[0];
  }

  void Clear(T&& val) {
    for (size_t i = 0; i < 3; ++i) data[i] = val;
  }

 private:
  std::array<T, 3> data;
};

struct EffectTemplate {
  PerPassData<ShaderPass*> pass_shaders;

  Assets::TransparencyMode transparency;
};

struct MaterialData {
  std::vector<SampledTexture> textures;
  std::string base_template;

  bool operator==(const MaterialData& other) const;

  size_t hash() const;
};

struct Material {
  EffectTemplate* original;
  PerPassData<VkDescriptorSet> pass_sets;

  std::vector<SampledTexture> textures;

  Material& operator=(const Material& other) = default;
};

class MaterialSystem {
 public:
  static void Init(Engine::VulkanEngine* engine);
  static void Cleanup();

  static void BuildDefaultTemplates();

  static ShaderPass* BuildShader(RenderPass* render_pass,
                                 PipelineBuilder& builder,
                                 ShaderEffect* effect);

  static Material* BuildMaterial(const std::string& name,
                                 const MaterialData& info);
  static Material* GetMaterial(const std::string& name);

  static void FillBuilders();

  inline static MaterialSystem& Get();

 private:
  static ShaderEffect* BuildEffect(std::string_view vertex_shader,
                                   std::string_view fragment_shader);

  struct MaterialInfoHash {
    size_t operator()(const MaterialData& data) const { return data.hash(); }
  };

  PipelineBuilder forward_builder_;
  PipelineBuilder shadow_builder_;

  std::unordered_map<std::string, EffectTemplate> template_cache_;
  std::unordered_map<std::string, Material*> materials_;
  std::unordered_map<MaterialData, Material*, MaterialInfoHash> material_cache_;

  Engine::VulkanEngine* engine_;
};

}