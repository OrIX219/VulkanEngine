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

ShaderEffect* MaterialSystem::BuildEffect(ShaderEffectStage vertex_shader,
                                          ShaderEffectStage fragment_shader,
                                          ShaderEffectStage geometry_shader) {
  MaterialSystem& system = Get();

  std::array<ShaderEffect::ReflectionOverrides, 1> overrides = {
      {"sceneData", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC}};

  ShaderEffect* effect = new ShaderEffect();

  effect->AddStage(system.engine_->shader_cache_.GetShader(
                       "Shaders/" + std::string{vertex_shader.shader_path}),
                   VK_SHADER_STAGE_VERTEX_BIT, vertex_shader.constants);
  if (fragment_shader.shader_path.size() > 2)
    effect->AddStage(system.engine_->shader_cache_.GetShader(
                         "Shaders/" + std::string{fragment_shader.shader_path}),
                     VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader.constants);
  if (geometry_shader.shader_path.size() > 2)
    effect->AddStage(system.engine_->shader_cache_.GetShader(
                         "Shaders/" + std::string{geometry_shader.shader_path}),
                     VK_SHADER_STAGE_GEOMETRY_BIT, geometry_shader.constants);

  effect->ReflectLayout(&system.engine_->device_, overrides.data(),
                        overrides.size());

  system.engine_->main_deletion_queue_.PushPointer(effect);

  return effect;
}

void MaterialSystem::BuildDefaultTemplates() {
  MaterialSystem& system = Get();

  FillBuilders();

  ShaderEffect* default_effect =
      BuildEffect({"default.vert.spv"}, {"default.frag.spv"});
  ShaderEffect* textured_lit =
      BuildEffect({"mesh_instanced.vert.spv"}, {"textured_lit.frag.spv"});
  ShaderEffect* textured_lit_emissive = BuildEffect(
      {"mesh_instanced.vert.spv"}, {"textured_lit_emissive.frag.spv"});
  ShaderEffect* textured_lit_normals = BuildEffect(
      {"mesh_instanced.vert.spv"}, {"textured_lit_normals.frag.spv"});
  ShaderEffect* opaque_shadowcast =
      BuildEffect({"shadowcast.vert.spv"}, {}, {"shadowcast.geom.spv"});
  ShaderEffect* opaque_shadowcast_point =
      BuildEffect({"shadowcast.vert.spv"}, {"shadowcast_point.frag.spv"},
                  {"shadowcast_point.geom.spv"});
  ShaderEffect* normals = BuildEffect(
      {"normals.vert.spv"}, {"normals.frag.spv"}, {"normals.geom.spv"});
  ShaderEffect* skybox = BuildEffect({"skybox.vert.spv"}, {"skybox.frag.spv"});

  ShaderPass* default_pass = BuildShader(
      system.engine_->forward_pass_, system.forward_builder_, default_effect);
  ShaderPass* default_wireframe_pass = BuildShader(
      system.engine_->forward_pass_, system.wireframe_builder_, default_effect);
  ShaderPass* textured_lit_pass = BuildShader(
      system.engine_->forward_pass_, system.forward_builder_, textured_lit);
  ShaderPass* textured_lit_emissive_pass =
      BuildShader(system.engine_->forward_pass_, system.forward_builder_,
                  textured_lit_emissive);
  ShaderPass* textured_lit_normals_pass =
      BuildShader(system.engine_->forward_pass_, system.forward_builder_,
                  textured_lit_normals);
  ShaderPass* opaque_shadowcast_pass =
      BuildShader(system.engine_->directional_shadow_pass_,
                  system.shadow_builder_, opaque_shadowcast);
  ShaderPass* opaque_shadowcast_point_pass =
      BuildShader(system.engine_->point_shadow_pass_, system.shadow_builder_,
                  opaque_shadowcast_point);
  ShaderPass* normals_pass = BuildShader(system.engine_->forward_pass_,
                                         system.forward_builder_, normals);
  ShaderPass* skybox_pass = BuildShader(system.engine_->forward_pass_,
                                        system.skybox_builder_, skybox);

  {
    EffectTemplate default_template;
    default_template.pass_shaders[MeshPassType::kForward] = default_pass;
    default_template.pass_shaders[MeshPassType::kTransparency] = nullptr;
    default_template.pass_shaders[MeshPassType::kDirectionalShadow] = nullptr;
    default_template.pass_shaders[MeshPassType::kPointShadow] = nullptr;
    default_template.pass_shaders[MeshPassType::kSpotShadow] = nullptr;

    default_template.transparency = Assets::TransparencyMode::kOpaque;

    system.template_cache_["default"] = default_template;
  }
  {
    EffectTemplate default_wireframe;
    default_wireframe.pass_shaders[MeshPassType::kForward] =
        default_wireframe_pass;
    default_wireframe.pass_shaders[MeshPassType::kTransparency] = nullptr;
    default_wireframe.pass_shaders[MeshPassType::kDirectionalShadow] = nullptr;
    default_wireframe.pass_shaders[MeshPassType::kPointShadow] = nullptr;
    default_wireframe.pass_shaders[MeshPassType::kSpotShadow] = nullptr;

    default_wireframe.transparency = Assets::TransparencyMode::kOpaque;

    system.template_cache_["default_wireframe"] = default_wireframe;
  }
  {
    EffectTemplate default_textured;
    default_textured.pass_shaders[MeshPassType::kForward] = textured_lit_pass;
    default_textured.pass_shaders[MeshPassType::kTransparency] = nullptr;
    default_textured.pass_shaders[MeshPassType::kDirectionalShadow] = opaque_shadowcast_pass;
    default_textured.pass_shaders[MeshPassType::kPointShadow] =
        opaque_shadowcast_point_pass;
    default_textured.pass_shaders[MeshPassType::kSpotShadow] = nullptr;

    default_textured.transparency = Assets::TransparencyMode::kOpaque;

    system.template_cache_["texturedPBR_opaque"] = default_textured;
  }
  {
    EffectTemplate textured_emissive;
    textured_emissive.pass_shaders[MeshPassType::kForward] =
        textured_lit_emissive_pass;
    textured_emissive.pass_shaders[MeshPassType::kTransparency] =
        nullptr;
    textured_emissive.pass_shaders[MeshPassType::kDirectionalShadow] =
        opaque_shadowcast_pass;
    textured_emissive.pass_shaders[MeshPassType::kPointShadow] =
        opaque_shadowcast_point_pass;
    textured_emissive.pass_shaders[MeshPassType::kSpotShadow] = nullptr;

    textured_emissive.transparency = Assets::TransparencyMode::kOpaque;

    system.template_cache_["texturedPBR_emissive"] = textured_emissive;
  }
  {
    EffectTemplate textured_normals;
    textured_normals.pass_shaders[MeshPassType::kForward] = textured_lit_normals_pass;
    textured_normals.pass_shaders[MeshPassType::kTransparency] = nullptr;
    textured_normals.pass_shaders[MeshPassType::kDirectionalShadow] =
        opaque_shadowcast_pass;
    textured_normals.pass_shaders[MeshPassType::kPointShadow] =
        opaque_shadowcast_point_pass;
    textured_normals.pass_shaders[MeshPassType::kSpotShadow] = nullptr;

    textured_normals.transparency = Assets::TransparencyMode::kOpaque;

    system.template_cache_["texturedNormals"] = textured_normals;
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
    transparend_forward.SetColorBlendAttachment(0, &blend)
        .SetDepthStencil(VK_TRUE, VK_FALSE)
        .SetRasterizer(VK_POLYGON_MODE_FILL, 1.f, VK_CULL_MODE_NONE);

    ShaderPass* transparent_lit_pass = BuildShader(
        system.engine_->forward_pass_, transparend_forward, textured_lit);

    EffectTemplate default_textured_transparent;
    default_textured_transparent.pass_shaders[MeshPassType::kForward] = nullptr;
    default_textured_transparent.pass_shaders[MeshPassType::kTransparency] =
        transparent_lit_pass;
    default_textured_transparent
        .pass_shaders[MeshPassType::kDirectionalShadow] = nullptr;
    default_textured_transparent.pass_shaders[MeshPassType::kPointShadow] =
        nullptr;
    default_textured_transparent.pass_shaders[MeshPassType::kSpotShadow] =
        nullptr;

    default_textured_transparent.transparency =
        Assets::TransparencyMode::kTransparent;

    system.template_cache_["texturedPBR_transparent"] =
        default_textured_transparent;
  }
  {
    EffectTemplate normals_template;
    normals_template.pass_shaders[MeshPassType::kForward] = normals_pass;
    normals_template.pass_shaders[MeshPassType::kTransparency] = nullptr;
    normals_template.pass_shaders[MeshPassType::kDirectionalShadow] =
        nullptr;
    normals_template.pass_shaders[MeshPassType::kPointShadow] = nullptr;
    normals_template.pass_shaders[MeshPassType::kSpotShadow] = nullptr;

    normals_template.transparency = Assets::TransparencyMode::kOpaque;

    system.template_cache_["normals"] = normals_template;
  }
  {
    EffectTemplate skybox_template;
    skybox_template.pass_shaders[MeshPassType::kForward] = skybox_pass;
    skybox_template.pass_shaders[MeshPassType::kTransparency] = nullptr;
    skybox_template.pass_shaders[MeshPassType::kDirectionalShadow] = nullptr;
    skybox_template.pass_shaders[MeshPassType::kPointShadow] = nullptr;
    skybox_template.pass_shaders[MeshPassType::kSpotShadow] = nullptr;

    skybox_template.transparency = Assets::TransparencyMode::kOpaque;

    system.template_cache_["skybox"] = skybox_template;
  }
}

ShaderPass* MaterialSystem::BuildShader(RenderPass& render_pass,
                                        PipelineBuilder& builder,
                                        ShaderEffect* effect) {
  MaterialSystem& system = Get();

  ShaderPass* pass = new ShaderPass();
  pass->effect = effect;

  PipelineBuilder pipline_builder = builder;
  pipline_builder.SetShaders(effect);
  pass->pipeline = pipline_builder.Build(render_pass);

  system.engine_->main_deletion_queue_.PushPointer(pass);
  system.engine_->main_deletion_queue_.PushFunction(
      [=, pipeline = pass->pipeline.Get()]() {
    vkDestroyPipeline(system.engine_->device_.Get(), pipeline,
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
    system.engine_->main_deletion_queue_.PushPointer(new_material);

    DescriptorBuilder builder = DescriptorBuilder::Begin(
        &system.engine_->layout_cache_, &system.engine_->descriptor_allocator_);

    std::vector<VkDescriptorImageInfo> image_infos(info.textures.size());
    for (uint32_t i = 0; i < info.textures.size(); ++i) {
      VkDescriptorImageInfo image_info{};
      image_info.sampler = info.textures[i].sampler;
      image_info.imageView = info.textures[i].view;
      image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      image_infos[i] = image_info;
      builder.BindImage(i, &image_infos[i],
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

  system.wireframe_builder_ = PipelineBuilder::Begin(&system.engine_->device_);
  system.wireframe_builder_.SetDefaults()
      .SetVertexInputDescription(Vertex::GetDescription())
      .SetRasterizer(VK_POLYGON_MODE_LINE, 1.f, VK_CULL_MODE_NONE,
                     VK_FRONT_FACE_COUNTER_CLOCKWISE)
      .SetDepthStencil()
      .SetMultisampling(system.engine_->samples_, VK_TRUE);

  system.forward_builder_ = PipelineBuilder::Begin(&system.engine_->device_);
  system.forward_builder_.SetDefaults()
      .SetVertexInputDescription(Vertex::GetDescription())
      .SetRasterizer(VK_POLYGON_MODE_FILL, 1.f, VK_CULL_MODE_BACK_BIT,
                     VK_FRONT_FACE_COUNTER_CLOCKWISE)
      .SetDepthStencil()
      .SetMultisampling(system.engine_->samples_, VK_TRUE);

  system.shadow_builder_ = PipelineBuilder::Begin(&system.engine_->device_);
  system.shadow_builder_.SetDefaults()
      .SetVertexInputDescription(Vertex::GetDescription())
      .SetRasterizer(VK_POLYGON_MODE_FILL, 1.f, VK_CULL_MODE_NONE,
                     VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_TRUE)
      .SetDepthStencil()
      .SetMultisampling(VK_SAMPLE_COUNT_1_BIT, VK_TRUE);

  system.skybox_builder_ = PipelineBuilder::Begin(&system.engine_->device_);
  system.skybox_builder_.SetDefaults()
      .SetVertexInputDescription(Renderer::Vertex::GetDescription())
      .SetRasterizer(VK_POLYGON_MODE_FILL, 1.f, VK_CULL_MODE_BACK_BIT,
                     VK_FRONT_FACE_COUNTER_CLOCKWISE)
      .SetDepthStencil(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
      .SetMultisampling(system.engine_->samples_, VK_TRUE);
}

MaterialSystem& MaterialSystem::Get() {
  static MaterialSystem system{};
  return system;
}

}  // namespace Renderer