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
  Pipeline pipeline;
};

struct SampledTexture {
  VkSampler sampler;
  VkImageView view;
};

enum class MeshPassType {
  kForward,
  kTransparency,
  kDirectionalShadow,
  kPointShadow,
  kSpotShadow,
  _Size
};

template<typename T>
class PerPassData {
 public:
  T& operator[](MeshPassType pass) {
    return data[static_cast<uint32_t>(pass)];
  }

  void Clear(T&& val) {
    for (size_t i = 0; i < static_cast<size_t>(MeshPassType::_Size); ++i)
      data[i] = val;
  }

 private:
  std::array<T, static_cast<size_t>(MeshPassType::_Size)> data;
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

  static ShaderPass* BuildShader(RenderPass& render_pass,
                                 PipelineBuilder& builder,
                                 ShaderEffect* effect);

  static Material* BuildMaterial(const std::string& name,
                                 const MaterialData& info);
  static Material* GetMaterial(const std::string& name);

  static void FillBuilders();

  inline static MaterialSystem& Get();

 private:
  struct ShaderEffectStage {
    std::string_view shader_path;
    VkSpecializationInfo constants = {};
  };

  static ShaderEffect* BuildEffect(ShaderEffectStage vertex_shader,
                                   ShaderEffectStage fragment_shader = {},
                                   ShaderEffectStage geometry_shader = {});

  struct MaterialInfoHash {
    size_t operator()(const MaterialData& data) const { return data.hash(); }
  };

  PipelineBuilder wireframe_builder_;
  PipelineBuilder forward_builder_;
  PipelineBuilder shadow_builder_;
  PipelineBuilder skybox_builder_;

  std::unordered_map<std::string, EffectTemplate> template_cache_;
  std::unordered_map<std::string, Material*> materials_;
  std::unordered_map<MaterialData, Material*, MaterialInfoHash> material_cache_;

  Engine::VulkanEngine* engine_;
};

}