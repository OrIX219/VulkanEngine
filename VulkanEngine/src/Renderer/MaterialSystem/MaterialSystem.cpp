#include "MaterialSystem.h"

#include <string_view>

#include "VulkanEngine.h"
#include "Shaders.h"
#include "Logger.h" 

namespace Renderer {

bool MaterialData::operator==(const MaterialData& other) const {
  if (other.base_template != base_template ||
      other.textures.size() != textures.size()) {
    return false;
  }

  return (memcmp(other.textures.data(), textures.data(),
                 textures.size() * sizeof(textures[0])) == 0);
}

size_t MaterialData::hash() const {
  size_t result = std::hash<std::string>()(base_template);

  for (const SampledTexture& texture : textures) {
    size_t texture_hash =
        (std::hash<size_t>()(reinterpret_cast<size_t>(texture.sampler)) << 3) &&
        (std::hash<size_t>()(reinterpret_cast<size_t>(texture.view)) << 7);

    result ^= std::hash<size_t>()(texture_hash);
  }

  return result;
}

void MaterialSystem::Init(Engine::VulkanEngine* engine) {
  Get().engine_ = engine;
  BuildDefaultTemplates();
}

void MaterialSystem::Cleanup() {}

ShaderEffect* MaterialSystem::BuildEffect(std::string_view vertex_shader,
                                          std::string_view fragment_shader) {
  MaterialSystem& system = Get();

  std::array<ShaderEffect::ReflectionOverrides, 1> overrides = {
      {"sceneData", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC}};

  ShaderEffect* effect = new ShaderEffect();

  effect->AddStage(system.engine_->shader_cache_.GetShader(
                       "Shaders/" + std::string{vertex_shader}),
                   VK_SHADER_STAGE_VERTEX_BIT);
  if (fragment_shader.size() > 2)
    effect->AddStage(system.engine_->shader_cache_.GetShader(
                         "Shaders/" + std::string{fragment_shader}),
                     VK_SHADER_STAGE_FRAGMENT_BIT);

  effect->ReflectLayout(&system.engine_->device_, overrides.data(),
                        overrides.size());

  system.engine_->main_deletion_queue_.PushFunction(
      std::bind(&ShaderEffect::Destroy, effect));

  return effect;
}

void MaterialSystem::BuildDefaultTemplates() {
  MaterialSystem& system = Get();

  FillBuilders();

  ShaderEffect* textured_lit =
      BuildEffect("mesh_instanced.vert.spv", "textured_lit.frag.spv");
  ShaderEffect* default_lit =
      BuildEffect("mesh_instanced.vert.spv", "default_lit.frag.spv");

  ShaderPass* textured_lit_pass = BuildShader(
      &system.engine_->render_pass_, system.forward_builder_, textured_lit);
  ShaderPass* default_lit_pass = BuildShader(
      &system.engine_->render_pass_, system.forward_builder_, default_lit);

  {
    EffectTemplate default_textured;
    default_textured.pass_shaders[MeshPassType::kForward] = textured_lit_pass;
    default_textured.pass_shaders[MeshPassType::kTransparency] = nullptr;
    default_textured.pass_shaders[MeshPassType::kDirectionalShadow] = nullptr;

    default_textured.transparency = Assets::TransparencyMode::kOpaque;

    system.template_cache_["texturedPBR_opaque"] = default_textured;
  }
  {
    PipelineBuilder transparend_forward = system.forward_builder_;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT;
    transparend_forward.SetColorBlendAttachment(&blend)
        .SetDepthStencil(VK_TRUE, VK_FALSE)
        .SetRasterizer(VK_POLYGON_MODE_FILL, 1.f, VK_CULL_MODE_NONE);

    ShaderPass* transparent_lit_pass = BuildShader(
        &system.engine_->render_pass_, transparend_forward, textured_lit);

    EffectTemplate default_textured_transparent;
    default_textured_transparent.pass_shaders[MeshPassType::kForward] = nullptr;
    default_textured_transparent.pass_shaders[MeshPassType::kTransparency] =
        transparent_lit_pass;
    default_textured_transparent
        .pass_shaders[MeshPassType::kDirectionalShadow] = nullptr;

    default_textured_transparent.transparency =
        Assets::TransparencyMode::kTransparent;

    system.template_cache_["texturedPBR_transparent"] =
        default_textured_transparent;
  }
  {
    EffectTemplate default_colored;
    default_colored.pass_shaders[MeshPassType::kForward] = default_lit_pass;
    default_colored.pass_shaders[MeshPassType::kTransparency] = nullptr;
    default_colored.pass_shaders[MeshPassType::kDirectionalShadow] = nullptr;

    default_colored.transparency = Assets::TransparencyMode::kOpaque;

    system.template_cache_["colored_opaque"] = default_colored;
  }
}

ShaderPass* MaterialSystem::BuildShader(RenderPass* render_pass,
                                        PipelineBuilder& builder,
                                        ShaderEffect* effect) {
  MaterialSystem& system = Get();

  ShaderPass* pass = new ShaderPass();
  pass->effect = effect;
  pass->pipeline_layout = effect->built_layout;

  PipelineBuilder pipline_builder = builder;
  pipline_builder.SetShaders(effect);
  pass->pipeline = pipline_builder.Build(render_pass).GetPipeline();

  system.engine_->main_deletion_queue_.PushFunction([=]() {
    vkDestroyPipeline(system.engine_->device_.GetDevice(), pass->pipeline,
                      nullptr);
  });

  return pass;
}

Material* MaterialSystem::BuildMaterial(const std::string& name,
                                        const MaterialData& info) {
  MaterialSystem& system = Get();

  Material* material;
  auto iter = system.material_cache_.find(info);
  if (iter != system.material_cache_.end()) {
    material = iter->second;
    system.materials_[name] = material;
  } else {
    Material* new_material = new Material();
    new_material->original = &system.template_cache_[info.base_template];
    new_material->pass_sets[MeshPassType::kDirectionalShadow] = VK_NULL_HANDLE;
    new_material->textures = info.textures;

    DescriptorBuilder builder = DescriptorBuilder::Begin(
        &system.engine_->layout_cache_, &system.engine_->descriptor_allocator_);

    for (uint32_t i = 0; i < info.textures.size(); ++i) {
      VkDescriptorImageInfo image_info{};
      image_info.sampler = info.textures[i].sampler;
      image_info.imageView = info.textures[i].view;
      image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      builder.BindImage(i, &image_info,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    builder.Build(new_material->pass_sets[MeshPassType::kForward]);
    builder.Build(new_material->pass_sets[MeshPassType::kTransparency]);
    LOG_INFO("Built new material '{}'", name);

    system.material_cache_[info] = new_material;
    material = new_material;
    system.materials_[name] = material;
  }

  return material;
}

Material* MaterialSystem::GetMaterial(const std::string& name) {
  MaterialSystem& system = Get();

  auto iter = system.materials_.find(name);
  if (iter != system.materials_.end()) return iter->second;
  return nullptr;
}

void MaterialSystem::FillBuilders() {
  MaterialSystem& system = Get();

  system.forward_builder_ = PipelineBuilder::Begin(&system.engine_->device_);
  system.forward_builder_.SetDefaults()
      .SetVertexInputDescription(Vertex::GetDescription())
      .SetRasterizer(VK_POLYGON_MODE_FILL, 1.f, VK_CULL_MODE_BACK_BIT,
                     VK_FRONT_FACE_COUNTER_CLOCKWISE)
      .SetDepthStencil()
      .SetMultisampling(system.engine_->physical_device_.GetMaxSamples(),
                        VK_TRUE);
  system.shadow_builder_ = PipelineBuilder::Begin(&system.engine_->device_);
  system.shadow_builder_.SetDefaults()
      .SetVertexInputDescription(Vertex::GetDescription())
      .SetRasterizer(VK_POLYGON_MODE_FILL, 1.f, VK_CULL_MODE_BACK_BIT,
                     VK_FRONT_FACE_COUNTER_CLOCKWISE)
      .SetDepthStencil()
      .SetMultisampling(system.engine_->physical_device_.GetMaxSamples(),
                        VK_TRUE);
}

MaterialSystem& MaterialSystem::Get() {
  static MaterialSystem system{};
  return system;
}

}  // namespace Renderer