#include "VulkanEngine.h"

#include <iostream>
#include <functional>

#include <vulkan/vulkan.hpp>
#include <vulkan/vk_enum_string_helper.h>

#define VMA_IMPLEMENTATION
#include <vma\include\vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/transform.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "Console/CVAR.h"
#include "Logger.h"

#include "MaterialAsset.h"

#define VK_CHECK(x)                                                       \
  do {                                                                    \
    VkResult err = x;                                                     \
    if (err) {                                                            \
      LOG_FATAL(std::string_view{string_VkResult(err)});                  \
    }                                                                     \
  } while (0);

#ifdef NDEBUG
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

namespace Engine {

VulkanEngine::VulkanEngine()
    : is_initialized_(false),
      frame_number_(0),
      delta_time_(0),
      last_time_(0),
      cursor_enabled_(false),
      menu_opened_(false) {}

VulkanEngine::~VulkanEngine() {}

void VulkanEngine::Init() {
  Logger::Get().SetTime();
  LOG_INFO("Engine init");

  window_.Init(1600, 900, "Vulkan Engine", this);
  glfwSetInputMode(window_.GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  LOG_SUCCESS("Window created");
  VK_CHECK(instance_.Init(
      kEnableValidationLayers,
      {"VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_monitor"}));
  LOG_SUCCESS("Vulkan instance initialized");

  Logger::Init(&instance_);
  LOG_SUCCESS("Logger initialized");

  VK_CHECK(surface_.Init(&instance_, &window_));
  LOG_SUCCESS("GLFW surface initialized");
  VK_CHECK(
      physical_device_.Init(&instance_, &surface_,
                            {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                             VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME}));
  LOG_SUCCESS("GPU found");
  VK_CHECK(device_.Init(&physical_device_));
  LOG_SUCCESS("Logical device initialized");

  profiler_.Init(&device_,
                 physical_device_.GetProperties().limits.timestampPeriod);
  LOG_SUCCESS("Profiler initialized");
  InitCVars();
  LOG_SUCCESS("CVars system initialized");

  VmaAllocatorCreateInfo allocator_info{};
  allocator_info.instance = instance_.GetInstance();
  allocator_info.physicalDevice = physical_device_.GetDevice();
  allocator_info.device = device_.GetDevice();
  allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;

  VK_CHECK(vmaCreateAllocator(&allocator_info, &allocator_));

  Renderer::CommandPool init_pool;
  VK_CHECK(init_pool.Create(
      &device_, device_.GetQueueFamilies().graphics_family.value()));

  VK_CHECK(swapchain_.Create(&device_, &surface_));
  LOG_SUCCESS("Swapchain created");
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::Swapchain::Destroy, swapchain_));

  VkExtent2D extent = swapchain_.GetImageExtent();
  VK_CHECK(color_image_.Create(allocator_, &device_,
                               {extent.width, extent.height, 1},
                               VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                               1, physical_device_.GetMaxSamples()));
  LOG_SUCCESS("Color image created");
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::Image::Destroy, color_image_));
  VK_CHECK(depth_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 1,
      physical_device_.GetMaxSamples(), VK_FORMAT_D32_SFLOAT,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT));
  LOG_SUCCESS("Depth image created");
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::Image::Destroy, depth_image_));

  InitRenderPasses();
  LOG_SUCCESS("Render passes initialized");

  VK_CHECK(swapchain_framebuffers_.Create(&swapchain_, &render_pass_,
                                          &color_image_, &depth_image_));
  LOG_SUCCESS("Swapchain framebuffers created");
  main_deletion_queue_.PushFunction(std::bind(
      &Renderer::SwapchainFramebuffers::Destroy, swapchain_framebuffers_));

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    VK_CHECK(frames_[i].command_pool_.Create(
        &device_, device_.GetQueueFamilies().graphics_family.value()));
    main_deletion_queue_.PushFunction(
        std::bind(&Renderer::CommandPool::Destroy, frames_[i].command_pool_));
  }

  shader_cache_.Init(&device_);

  InitSyncStructures();
  InitDescriptors();
  LOG_SUCCESS("Descriptors initialized");
  Renderer::MaterialSystem::Init(this);
  LOG_SUCCESS("Material System initialized");

  Renderer::Queue& init_queue = device_.GetQueue(init_pool.GetQueueFamily());

  texture_sampler_.SetDefaults().Create(&device_);
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::TextureSampler::Destroy, texture_sampler_));

  init_queue.BeginBatch();
  InitScene(init_pool);
  init_queue.EndBatch();

  init_queue.SubmitBatches();

  InitImgui(init_pool);

  device_.WaitIdle();
  is_initialized_ = true;

  init_pool.Destroy();
}

void VulkanEngine::InitCVars() {
  AutoCVar_Vec4 CVar_clear_color("scene.clear_color", "Background color",
                                 {0.f, 0.f, 0.f, 1.f});
  AutoCVar_Vec4 CVar_ambient_color("scene.ambient_color", "Global color tint",
                                 {1.f, 1.f, 1.f, 1.f});

  const VkPhysicalDeviceProperties& props = physical_device_.GetProperties();
  AutoCVar_String CVar_device_type(
      "device_type", "Device type",
      string_VkPhysicalDeviceType(props.deviceType),
      CVarFlagBits::kEditReadOnly);
  AutoCVar_String CVar_device_name("device_name", "Device name",
                                   props.deviceName,
                                   CVarFlagBits::kEditReadOnly);
  AutoCVar_Int CVar_max_push_constant_size(
      "limits.max_push_constant_size", "Max Push Constant Size",
      props.limits.maxPushConstantsSize,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_memory_allocation_count(
      "limits.max_memory_allocation_count", "Max Memory Allocation Count",
      props.limits.maxMemoryAllocationCount,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_bound_descriptor_sets(
      "limits.max_bound_descriptor_sets", "Max Bound Descriptor Sets",
      props.limits.maxBoundDescriptorSets,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_descriptor_set_samplers(
      "limits.max_descriptor_set_samplers", "Max Descriptor Set Samplers",
      props.limits.maxDescriptorSetSamplers,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_descriptor_set_uniform_buffers(
      "limits.max_descriptor_set_uniform_buffers",
      "Max Descriptor Set Uniform Buffers",
      props.limits.maxDescriptorSetUniformBuffers,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_descriptor_set_dynamic_uniform_buffers(
      "limits.max_descriptor_set_dynamic_uniform_buffers",
      "Max Descriptor Set Dynamic Uniform Buffers",
      props.limits.maxDescriptorSetUniformBuffersDynamic,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_descriptor_set_storage_buffers(
      "limits.max_descriptor_set_storage_buffers",
      "Max Descriptor Set Storage Buffers",
      props.limits.maxDescriptorSetStorageBuffers,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_descriptor_set_dynamic_storage_buffers(
      "limits.max_descriptor_set_dynamic_storage_buffers",
      "Max Descriptor Set Dynamic Storage Buffers",
      props.limits.maxDescriptorSetStorageBuffersDynamic,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_descriptor_set_sampled_images(
      "limits.max_descriptor_set_sampled_images",
      "Max Descriptor Set Sampled Images",
      props.limits.maxDescriptorSetSampledImages,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_descriptor_set_storage_images(
      "limits.max_descriptor_set_storage_images",
      "Max Descriptor Set Storage Images",
      props.limits.maxDescriptorSetStorageImages,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_descriptor_set_input_attachments(
      "limits.max_descriptor_set_input_attachments",
      "Max Descriptor Set Input Attachments",
      props.limits.maxDescriptorSetInputAttachments,
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);
  AutoCVar_Int CVar_max_sample_count(
      "limits.max_sample_count", "Max Sample Count",
      physical_device_.GetMaxSamples(),
      CVarFlagBits::kEditReadOnly | CVarFlagBits::kAdvanced);

  AutoCVar_String CVar_asset_path("assets.path", "Path to assets",
                                  "asset_export", CVarFlagBits::kAdvanced);
}

void VulkanEngine::InitRenderPasses() {
  Renderer::RenderPassBuilder render_pass_builder(&device_);

  Renderer::RenderPassAttachment color_attachment, depth_attachment,
      color_attachment_resolve;
  color_attachment
      .SetOperations(VK_ATTACHMENT_LOAD_OP_CLEAR,
                     VK_ATTACHMENT_STORE_OP_DONT_CARE)
      .SetSamples(physical_device_.GetMaxSamples())
      .SetLayouts(VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      .SetFormat(swapchain_.GetImageFormat());
  depth_attachment
      .SetOperations(VK_ATTACHMENT_LOAD_OP_CLEAR,
                     VK_ATTACHMENT_STORE_OP_DONT_CARE)
      .SetSamples(physical_device_.GetMaxSamples())
      .SetLayouts(VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      .SetFormat(VK_FORMAT_D32_SFLOAT);
  color_attachment_resolve.SetDefaults().SetFormat(swapchain_.GetImageFormat());

  render_pass_builder.AddAttachment(&color_attachment)
      .AddAttachment(&depth_attachment)
      .AddAttachment(&color_attachment_resolve);

  Renderer::RenderPassSubpass subpass;
  subpass.AddColorAttachmentRef(0)
      .SetDepthStencilAttachmentRef(1)
      .AddResolveAttachmentRef(2);

  render_pass_builder.AddSubpass(&subpass);
  render_pass_builder
      .AddDependency(VK_SUBPASS_EXTERNAL, 0,
                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
      .AddDependency(VK_SUBPASS_EXTERNAL, 0,
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                     0,
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

  render_pass_ = render_pass_builder.Build();

  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::RenderPass::Destroy, render_pass_));
}

void VulkanEngine::InitSyncStructures() {
  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    VK_CHECK(vkCreateFence(device_.GetDevice(), &fence_info, nullptr,
                           &frames_[i].render_fence_));
    VK_CHECK(vkCreateSemaphore(device_.GetDevice(), &semaphore_info, nullptr,
                               &frames_[i].render_semaphore_));
    VK_CHECK(vkCreateSemaphore(device_.GetDevice(), &semaphore_info, nullptr,
                               &frames_[i].present_semaphore_));

    main_deletion_queue_.PushFunction([=]() {
      vkDestroyFence(device_.GetDevice(), frames_[i].render_fence_, nullptr);
      vkDestroySemaphore(device_.GetDevice(), frames_[i].render_semaphore_,
                         nullptr);
      vkDestroySemaphore(device_.GetDevice(), frames_[i].present_semaphore_,
                         nullptr);
    });
  }
}

void VulkanEngine::InitDescriptors() {
  descriptor_allocator_.Init(&device_);
  layout_cache_.Init(&device_);

  const uint32_t kMaxObjects = 10000;
  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    VK_CHECK(frames_[i].object_buffer_.Create(
        allocator_, sizeof(ObjectData) * kMaxObjects,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT));
    main_deletion_queue_.PushFunction(
        std::bind(&Renderer::Buffer<true>::Destroy, frames_[i].object_buffer_));
  }

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    frames_[i].dynamic_data_.Create(
        allocator_, 8192,
        static_cast<uint32_t>(physical_device_.GetProperties()
                                  .limits.minUniformBufferOffsetAlignment));
    main_deletion_queue_.PushFunction(
        std::bind(&Renderer::PushBuffer::Destroy, frames_[i].dynamic_data_));

    VkDescriptorBufferInfo scene_info{};
    scene_info.buffer = frames_[i].dynamic_data_.GetBuffer();
    scene_info.offset = 0;
    scene_info.range = sizeof(SceneData);

    VkDescriptorBufferInfo object_info{};
    object_info.buffer = frames_[i].object_buffer_.GetBuffer();
    object_info.offset = 0;
    object_info.range = sizeof(ObjectData) * kMaxObjects;

    Renderer::DescriptorBuilder::Begin(&layout_cache_, &descriptor_allocator_)
        .BindBuffer(0, &scene_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build(frames_[i].global_descriptor_);

    Renderer::DescriptorBuilder::Begin(&layout_cache_, &descriptor_allocator_)
        .BindBuffer(0, &object_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_SHADER_STAGE_VERTEX_BIT)
        .Build(frames_[i].object_descriptor_);
  }
}

bool VulkanEngine::LoadMesh(Renderer::CommandBuffer command_buffer,
                            const char* name, const char* path) {
  Renderer::Mesh mesh{};
  bool loaded = mesh.LoadFromAsset(allocator_, command_buffer, path);
  if (!loaded) {
    LOG_ERROR("Failed to load mesh '{}' from {}", name, path);
    return false;
  } else {
    LOG_SUCCESS("Loaded mesh '{}'", name);
  }
  meshes_[name] = mesh;
  main_deletion_queue_.PushFunction(std::bind(&Renderer::Mesh::Destroy, mesh));

  return true;
}

bool VulkanEngine::LoadTexture(Renderer::CommandBuffer command_buffer,
                               const char* name, const char* path) {
  Renderer::Texture texture{};
  bool loaded =
      texture.LoadFromAsset(allocator_, &device_, command_buffer, path);
  if (!loaded) {
    LOG_ERROR("Failed to load texture '{}' from {}", name, path);
    return false;
  } else {
    LOG_SUCCESS("Loaded texture '{}'", name);
  }
  textures_[name] = texture;
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::Texture::Destroy, texture));

  return true;
}

bool VulkanEngine::LoadPrefab(Renderer::CommandBuffer command_buffer,
                              const char* path, glm::mat4 root) {
  if (prefab_cache_.find(path) == prefab_cache_.end()) {
    Assets::AssetFile file;
    bool loaded = Assets::LoadBinaryFile(path, file);

    if (!loaded) {
      LOG_ERROR("Failed to load prefab '{}'", path);
      return false;
    } else {
      LOG_SUCCESS("Loaded prefab '{}'", path);
    }

    prefab_cache_[path] = new Assets::PrefabInfo;
    *prefab_cache_[path] = Assets::ReadPrefabInfo(&file);
  }

  Assets::PrefabInfo* info = prefab_cache_[path];

  std::unordered_map<uint64_t, glm::mat4> node_world_mats;
  std::vector<std::pair<uint64_t, glm::mat4>> pending_nodes;
  for (auto& [key, value] : info->node_matrices) {
    glm::mat4 node_mat{1.f};
    const std::array<float, 16>& mat = info->matrices[value];
    memcpy(&node_mat, mat.data(), sizeof(glm::mat4));

    auto iter = info->node_parents.find(key);
    if (iter == info->node_parents.end())
      node_world_mats[key] = root * node_mat;
    else
      pending_nodes.push_back({key, node_mat});
  }

  while (pending_nodes.size() > 0) {
    for (size_t i = 0; i < pending_nodes.size(); ++i) {
      uint64_t node = pending_nodes[i].first;
      uint64_t parent = info->node_parents[node];

      auto iter = node_world_mats.find(parent);
      if (iter != node_world_mats.end()) {
        glm::mat4 node_mat = iter->second * pending_nodes[i].second;
        node_world_mats[node] = node_mat;

        pending_nodes[i] = pending_nodes.back();
        pending_nodes.pop_back();
        --i;
      }
    }
  }

  for (auto& [key, value] : info->node_meshes) {
    if (!GetMesh(value.mesh_path)) {
      LoadMesh(command_buffer, value.mesh_path.c_str(),
               AssetPath(value.mesh_path).c_str());
    }

    Renderer::Material* material =
        Renderer::MaterialSystem::GetMaterial(value.material_path);

    if (!material) {
      Assets::AssetFile material_file;
      std::string material_path = AssetPath(value.material_path);
      bool loaded =
          Assets::LoadBinaryFile(material_path.c_str(), material_file);
      if (!loaded) {
        LOG_ERROR("Failed to load material '{}' from '{}'",
                  value.material_path.c_str(), material_path.c_str());
        return false;
      } else {
        LOG_SUCCESS("Loaded material '{}'", value.material_path.c_str());
      }

      Assets::MaterialInfo material_info =
          Assets::ReadMaterialInfo(&material_file);

      std::string texture;
      if (material_info.textures.size() == 0)
        texture = "white.tx";
      else
        texture = material_info.textures["base_color"];

      LoadTexture(command_buffer, texture.c_str(), AssetPath(texture).c_str());

      Renderer::SampledTexture tex;
      tex.sampler = texture_sampler_.GetSampler();
      tex.view = textures_[texture].GetView();

      Renderer::MaterialData info;
      if (material_info.transparency == Assets::TransparencyMode::kTransparent)
        info.base_template = "texturedPBR_transparent";
      else
        info.base_template = "texturedPBR_opaque";

      info.textures.push_back(tex);

      material =
          Renderer::MaterialSystem::BuildMaterial(value.material_path, info);

      if (!material)
        LOG_ERROR("Failed to build material '{}'", value.material_path);
    }

    Renderer::RenderObject object;
    glm::mat4 node_matrix{1.f};

    auto matrix_iter = node_world_mats.find(key);
    if (matrix_iter != node_world_mats.end()) {
      memcpy(&node_matrix, &(matrix_iter->second), sizeof(glm::mat4));
    }

    object.Create(GetMesh(value.mesh_path), material);
    object.ModelMatrix() = node_matrix;

    renderables_.push_back(object);
  }

  return true;
}

void VulkanEngine::InitScene(Renderer::CommandPool& init_pool) {
  Renderer::CommandBuffer command_buffer = init_pool.GetBuffer();
  command_buffer.Begin();

  LoadTexture(command_buffer, "white", AssetPath("white.tx").c_str());

  Renderer::SampledTexture white_texture;
  white_texture.sampler = texture_sampler_.GetSampler();
  white_texture.view = textures_["white"].GetView();

  Renderer::MaterialData material_info;
  material_info.base_template = "texturedPBR_opaque";
  material_info.textures.push_back(white_texture);
  Renderer::MaterialSystem::BuildMaterial("default", material_info);

  Renderer::MaterialData textured_info;
  textured_info.base_template = "texturedPBR_opaque";
  textured_info.textures.push_back(white_texture);
  Renderer::MaterialSystem::BuildMaterial("textured", textured_info);

  glm::mat4 room_root = glm::translate(glm::mat4{1.f}, glm::vec3(0, 0, -5));
  room_root =
      glm::rotate(room_root, glm::radians(-45.f), glm::vec3(0.f, 1.f, 0.f));
  room_root =
      glm::rotate(room_root, glm::radians(-45.f), glm::vec3(0.f, 0.f, 1.f));
  LoadPrefab(command_buffer, AssetPath("viking_room.pfb").c_str(), room_root);
  // LoadPrefab(command_buffer, AssetPath("star.pfb").c_str(), glm::mat4{1.f});
  LoadPrefab(command_buffer, AssetPath("star_untextured.pfb").c_str());
  LoadPrefab(command_buffer, AssetPath("two_stars.pfb").c_str());

  command_buffer.End();
  command_buffer.AddToBatch();
}

void VulkanEngine::InitImgui(Renderer::CommandPool& init_pool) {
  constexpr uint32_t kMaxCount = 128;

  std::vector<VkDescriptorPoolSize> sizes = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, kMaxCount},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxCount},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kMaxCount},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kMaxCount},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, kMaxCount},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, kMaxCount},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxCount},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kMaxCount},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kMaxCount},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, kMaxCount},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, kMaxCount}};
  
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = static_cast<uint32_t>(sizes.size());
  pool_info.pPoolSizes = sizes.data();
  pool_info.maxSets = kMaxCount;

  VK_CHECK(vkCreateDescriptorPool(device_.GetDevice(), &pool_info, nullptr,
                                  &imgui_pool_));
  main_deletion_queue_.PushFunction([=]() {
    vkDestroyDescriptorPool(device_.GetDevice(), imgui_pool_, nullptr);
  });

  ImGui::CreateContext();

  ImGui_ImplGlfw_InitForVulkan(window_.GetWindow(), true);

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = instance_.GetInstance();
  init_info.PhysicalDevice = physical_device_.GetDevice();
  init_info.Device = device_.GetDevice();
  init_info.Queue = device_.GetGraphicsQueue().GetQueue();
  init_info.DescriptorPool = imgui_pool_;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.MSAASamples = physical_device_.GetMaxSamples();

  ImGui_ImplVulkan_Init(&init_info, render_pass_.GetRenderPass());

  Renderer::CommandBuffer command_buffer = init_pool.GetBuffer();
  command_buffer.Begin();
  ImGui_ImplVulkan_CreateFontsTexture(command_buffer.GetBuffer());
  command_buffer.End();
  command_buffer.Submit();
  device_.GetGraphicsQueue().SubmitBatches();
  device_.GetGraphicsQueue().WaitIdle();

  ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void VulkanEngine::RecreateSwapchain() {
  VkExtent2D extent = window_.GetFramebufferSize();
  if (extent.width == 0 || extent.height == 0) {
    extent = window_.GetFramebufferSize();
    window_.WaitEvents();
  }

  device_.WaitIdle();

  swapchain_.Recreate();
  depth_image_.Destroy();
  extent = swapchain_.GetImageExtent();
  VK_CHECK(color_image_.Create(allocator_, &device_,
                               {extent.width, extent.height, 1}, 1,
                               VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                               physical_device_.GetMaxSamples()));
  depth_image_.Create(allocator_, &device_, {extent.width, extent.height, 1},
                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 1,
                      physical_device_.GetMaxSamples(), VK_FORMAT_D32_SFLOAT,
                      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
  swapchain_framebuffers_.Recreate();
}

std::string VulkanEngine::AssetPath(std::string_view path) {
  return *CVarSystem::Get()->GetStringCVar("assets.path") + '/' +
         std::string{path};
}

Renderer::Mesh* VulkanEngine::GetMesh(const std::string& name) {
  auto iter = meshes_.find(name);
  if (iter == meshes_.end()) return nullptr;
  return &(*iter).second;
}

void VulkanEngine::MousePosCallback(double x, double y) {
  static bool first_mouse = true;
  if (cursor_enabled_) return;

  if (first_mouse) {
    last_mouse_x_ = static_cast<int>(x);
    last_mouse_y_ = static_cast<int>(y);
    first_mouse = false;
  }

  float delta_x = static_cast<float>(x - last_mouse_x_);
  float delta_y = static_cast<float>(last_mouse_y_ - y);
  last_mouse_x_ = static_cast<int>(x);
  last_mouse_y_ = static_cast<int>(y);

  camera_.ProcessMouse(delta_x, delta_y);
}

void VulkanEngine::KeyCallback(int key, int action, int mods) {
  if (action == GLFW_PRESS) {
    switch (key) {
      case GLFW_KEY_ESCAPE:
        menu_opened_ = !menu_opened_;
        EnableCursor(menu_opened_);
        break;
      case GLFW_KEY_LEFT_ALT:
        EnableCursor(!cursor_enabled_);
        break;
    }
  }
}

void VulkanEngine::EnableCursor(bool enable) {
  if (enable == cursor_enabled_) return;
  if (enable) {
    cursor_enabled_ = true;
    glfwSetInputMode(window_.GetWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    VkExtent2D extent = window_.GetFramebufferSize();
    glfwSetCursorPos(window_.GetWindow(), extent.width / 2, extent.height / 2);
  } else {
    cursor_enabled_ = false;
    glfwSetCursorPos(window_.GetWindow(), last_mouse_x_, last_mouse_y_);
    glfwSetInputMode(window_.GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  }
}

void VulkanEngine::ProcessInput() {
  if (cursor_enabled_) return;

  if (glfwGetKey(window_.GetWindow(), GLFW_KEY_W) == GLFW_PRESS)
    camera_.ProcessKeyboard(Renderer::Camera::Direction::kForward, delta_time_);
  if (glfwGetKey(window_.GetWindow(), GLFW_KEY_S) == GLFW_PRESS)
    camera_.ProcessKeyboard(Renderer::Camera::Direction::kBackward,
                            delta_time_);
  if (glfwGetKey(window_.GetWindow(), GLFW_KEY_D) == GLFW_PRESS)
    camera_.ProcessKeyboard(Renderer::Camera::Direction::kRight, delta_time_);
  if (glfwGetKey(window_.GetWindow(), GLFW_KEY_A) == GLFW_PRESS)
    camera_.ProcessKeyboard(Renderer::Camera::Direction::kLeft, delta_time_);
  if (glfwGetKey(window_.GetWindow(), GLFW_KEY_SPACE) == GLFW_PRESS)
    camera_.ProcessKeyboard(Renderer::Camera::Direction::kUp, delta_time_);
  if (glfwGetKey(window_.GetWindow(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
    camera_.ProcessKeyboard(Renderer::Camera::Direction::kDown, delta_time_);
}

void VulkanEngine::Cleanup() {
  if (is_initialized_) {
    ImGui_ImplVulkan_Shutdown();

    main_deletion_queue_.Flush();

    Renderer::MaterialSystem::Cleanup();

    shader_cache_.Destroy();

    layout_cache_.Destroy();
    descriptor_allocator_.Destroy();

    vmaDestroyAllocator(allocator_);

    profiler_.Destroy();

    device_.Destroy();
    surface_.Destroy();

    Logger::Cleanup();
    instance_.Destroy();

    window_.Destroy();
  }
}

void VulkanEngine::Draw() {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  VK_CHECK(vkWaitForFences(device_.GetDevice(), 1, &frame.render_fence_,
                           VK_TRUE, UINT64_MAX));

  uint32_t image_index;
  VkResult res =
      swapchain_.AcquireNextImage(&image_index, frame.present_semaphore_);

  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapchain();
    return;
  } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
    VK_CHECK(res);
  }

  VK_CHECK(vkResetFences(device_.GetDevice(), 1, &frame.render_fence_));

  VK_CHECK(frame.command_pool_.Reset());
  frame.dynamic_data_.Reset();

  ImGui::Render();

  Renderer::CommandBuffer command_buffer = frame.command_pool_.GetBuffer();
  VK_CHECK(command_buffer.Begin());

  while (!prefabs_to_load_.empty()) {
    LoadPrefab(command_buffer, AssetPath(prefabs_to_load_.back()).c_str(),
               glm::mat4{1.f});
    prefabs_to_load_.pop_back();
  }

  VkClearValue clear_value{};
  auto clear_color = CVarSystem::Get()->GetVec4CVar("scene.clear_color");
  clear_value.color = {
      {clear_color->x, clear_color->y, clear_color->z, clear_color->w}};
  VkClearValue depth_clear{};
  depth_clear.depthStencil.depth = 1.f;

  profiler_.GrabQueries(command_buffer);
  {
    Renderer::VulkanScopeTimer timer(command_buffer, &profiler_, "Full Frame");
    Renderer::VulkanPipelineStatRecorder stats(command_buffer, &profiler_,
                                               "Total Primitives");

    render_pass_.Begin(command_buffer,
                       swapchain_framebuffers_.GetFramebuffer(image_index),
                       {{0, 0}, swapchain_.GetImageExtent()},
                       {clear_value, depth_clear, clear_value});

    VkExtent2D extent = swapchain_.GetImageExtent();
    VkViewport viewport{0.f, 0.f, (float)extent.width, (float)extent.height,
                        0.f, 1.f};
    VkRect2D scissors{{0, 0}, extent};

    vkCmdSetViewport(command_buffer.GetBuffer(), 0, 1, &viewport);
    vkCmdSetScissor(command_buffer.GetBuffer(), 0, 1, &scissors);

    DrawObjects(command_buffer, renderables_.data(), renderables_.size());

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                    command_buffer.GetBuffer());

    render_pass_.End(command_buffer);
  }

  VK_CHECK(command_buffer.End());

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &frame.present_semaphore_;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &frame.render_semaphore_;
  command_buffer.Submit(std::move(submit_info));

  VK_CHECK(device_.GetGraphicsQueue().SubmitBatches(frame.render_fence_));

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  VkSwapchainKHR swapchains[] = {swapchain_.GetSwapchain()};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &frame.render_semaphore_;
  present_info.pImageIndices = &image_index;

  res = device_.GetPresentQueue().Present(&present_info);

  if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR ||
      window_.GetResized()) {
    window_.SetResized(false);
    RecreateSwapchain();
  } else if (res != VK_SUCCESS) {
    VK_CHECK(res);
  }

  frame_number_++;
}

void VulkanEngine::DrawObjects(Renderer::CommandBuffer command_buffer,
                               Renderer::RenderObject* first, size_t count) {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  glm::mat4 projection =
      glm::perspective(glm::radians(90.f), 1600.f / 900.f, 0.1f, 100.f);
  projection[1][1] *= -1;

  scene_data_.camera_data.view = camera_.GetViewMat();
  scene_data_.camera_data.projection = projection;

  auto ambient_color = CVarSystem::Get()->GetVec4CVar("scene.ambient_color");
  scene_data_.ambient_color = {ambient_color->x, ambient_color->y,
                               ambient_color->z, ambient_color->w};

  uint32_t uniform_offset = frame.dynamic_data_.Push(scene_data_);

  ObjectData* object_SSBO =
      static_cast<ObjectData*>(frame.object_buffer_.GetMappedMemory());
  for (size_t i = 0; i < count; ++i) {
    Renderer::RenderObject& object = first[i];
    object_SSBO[i].model_matrix = object.ModelMatrix();
  }

  Renderer::Mesh* last_mesh = nullptr;
  Renderer::Material* last_material = nullptr;
  VkPipeline last_pipeline = nullptr;
  VkDescriptorSet last_material_set = nullptr;
  for (uint32_t i = 0; i < count; ++i) {
    Renderer::RenderObject& object = first[i];

    Renderer::Mesh* new_mesh = object.GetMesh();
    Renderer::Material* new_material = object.GetMaterial();
    VkPipeline new_pipeline =
        new_material->original->pass_shaders[Renderer::MeshPassType::kForward]
            ->pipeline;
    VkPipelineLayout new_layout =
        new_material->original->pass_shaders[Renderer::MeshPassType::kForward]
            ->pipeline_layout;
    VkDescriptorSet new_material_set =
        new_material->pass_sets[Renderer::MeshPassType::kForward];

    if (new_pipeline != last_pipeline) {
      last_pipeline = new_pipeline;
      vkCmdBindPipeline(command_buffer.GetBuffer(),
                        VK_PIPELINE_BIND_POINT_GRAPHICS, new_pipeline);
      vkCmdBindDescriptorSets(command_buffer.GetBuffer(),
                              VK_PIPELINE_BIND_POINT_GRAPHICS, new_layout, 0, 1,
                              &frame.global_descriptor_, 1, &uniform_offset);
      vkCmdBindDescriptorSets(command_buffer.GetBuffer(),
                              VK_PIPELINE_BIND_POINT_GRAPHICS, new_layout, 1, 1,
                              &frame.object_descriptor_, 0, nullptr);
    }

    if (new_material_set != last_material_set) {
      last_material_set = new_material_set;
      vkCmdBindDescriptorSets(command_buffer.GetBuffer(),
                              VK_PIPELINE_BIND_POINT_GRAPHICS, new_layout, 2, 1,
                              &new_material_set, 0, nullptr);
    }

    if (object.GetMesh() != last_mesh) {
      object.GetMesh()->BindBuffers(command_buffer);
      last_mesh = object.GetMesh();
    }

    vkCmdDrawIndexed(command_buffer.GetBuffer(),
                     object.GetMesh()->GetIndicesCount(), 1, 0, 0, i);
  }
}

void VulkanEngine::DrawMenu() {
  ImGui::SetNextWindowSize(ImVec2{100.f, 0.f});
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
  ImGui::Begin("Menu", &menu_opened_,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
  ImVec2 region = ImGui::GetContentRegionAvail();
  if (ImGui::Button("Exit", ImVec2{region.x, 0.f})) {
    window_.Close();
  }

  ImGui::End();
}

void VulkanEngine::DrawToolbar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Debug")) {
      if (ImGui::BeginMenu("CVAR")) {
        CVarSystem::Get()->DrawImguiEditor();
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Timings")) {
        for (auto& [k, v] : profiler_.timings) {
          ImGui::Text("%s %f ms", k.c_str(), v);
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Stats")) {
        for (auto& [k, v] : profiler_.stats) {
          ImGui::Text("%s %d", k.c_str(), v);
        }
        ImGui::EndMenu();
      }
      ImGui::EndMenu();
    }

    bool open_popup = false;
    if (ImGui::BeginMenu("Scene")) {
      if (ImGui::MenuItem("Load Prefab")) open_popup = true;
      ImGui::EndMenu();
    }
    if (open_popup) {
      ImGui::OpenPopup("Load Prefab");
      open_popup = false;
    }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Load Prefab", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      static std::string path;
      ImGui::Text("Path to prefab (relative to assets path):");
      ImGui::PushID("load_prefab");
      ImGui::PushItemWidth(-1.f);
      bool input =
          ImGui::InputTextWithHint("", "example.pfb", &path,
                                   ImGuiInputTextFlags_EnterReturnsTrue |
                                       ImGuiInputTextFlags_AutoSelectAll);
      ImGui::PopItemWidth();
      ImGui::PopID();

      if ((input || ImGui::Button("Load", ImVec2(150.f, 0.f))) &&
          path.size() > 0) {
        prefabs_to_load_.push_back(path);
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      ImGui::SetItemDefaultFocus();
      if (ImGui::Button("Cancel", ImVec2(150.f, 0.f))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    ImGui::EndMainMenuBar();
  }
}

void VulkanEngine::Run() {
  while (!window_.ShouldClose()) {
    window_.PollEvents();

    float current_time = static_cast<float>(glfwGetTime());
    delta_time_ = current_time - last_time_;
    last_time_ = current_time;

    ProcessInput();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (menu_opened_) DrawMenu();
    DrawToolbar();

    Draw();
  }

  device_.WaitIdle();
}

}  // namespace Engine