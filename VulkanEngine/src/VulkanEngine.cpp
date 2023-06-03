#include "VulkanEngine.h"

#include <iostream>
#include <functional>

#include <vulkan/vulkan.hpp>
#include <vulkan/vk_enum_string_helper.h>

#define VMA_IMPLEMENTATION
#include <vma\vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/transform.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include "CVAR.h"
#include "Logger.h"

#define VK_CHECK(x)                                                       \
  do {                                                                    \
    VkResult err = x;                                                     \
    if (err) {                                                            \
      std::cerr << "Vulkan error: " << string_VkResult(err) << std::endl; \
      abort();                                                            \
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
      menu_opened_(false),
      console_opened_(false) {}

VulkanEngine::~VulkanEngine() {}

void VulkanEngine::Init() {
  Logger::Get().SetTime();

  window_.Init(1600, 900, "Vulkan Engine", this);
  glfwSetInputMode(window_.GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  VK_CHECK(
      instance_.Init(kEnableValidationLayers, {"VK_LAYER_KHRONOS_validation"}));
  VK_CHECK(surface_.Init(&instance_, &window_));
  VK_CHECK(
      physical_device_.Init(&instance_, &surface_,
                            {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                             VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME}));
  VK_CHECK(device_.Init(&physical_device_));

  Logger::Init(&instance_);
  InitCVars();

  VmaAllocatorCreateInfo allocator_info{};
  allocator_info.instance = instance_.GetInstance();
  allocator_info.physicalDevice = physical_device_.GetDevice();
  allocator_info.device = device_.GetDevice();
  allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;

  VK_CHECK(vmaCreateAllocator(&allocator_info, &allocator_));
  VK_CHECK(init_pool_.Create(
      &device_, device_.GetQueueFamilies().graphics_family.value()));

  VK_CHECK(swapchain_.Create(&device_, &surface_));

  VkExtent2D extent = swapchain_.GetImageExtent();
  VK_CHECK(color_image_.Create(allocator_, &device_,
                               {extent.width, extent.height, 1},
                               VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                               1, physical_device_.GetMaxSamples()));
  VK_CHECK(depth_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 1,
      physical_device_.GetMaxSamples(), VK_FORMAT_D32_SFLOAT,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT));

  InitRenderPasses();

  VK_CHECK(swapchain_framebuffers_.Create(&swapchain_, &render_pass_,
                                          &color_image_, &depth_image_));

  for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    VK_CHECK(frames_[i].command_pool_.Create(
        &device_, device_.GetQueueFamilies().graphics_family.value()));

  InitSyncStructures();
  InitDescriptors();
  InitPipelines();

  Renderer::Queue& init_queue = device_.GetQueue(init_pool_.GetQueueFamily());

  init_queue.BeginBatch();
  LoadMeshes();
  LoadTextures();
  init_queue.EndBatch();

  init_queue.SubmitBatches();

  InitScene();

  InitImgui();

  device_.WaitIdle();
  is_initialized_ = true;

  init_pool_.Destroy();
}

void VulkanEngine::InitCVars() {
  AutoCVar_Float CVAR_clear_r("clear_color.r",
                              "Framebuffer clear color's red component", 0.f,
                              CVarFlags::kEditFloatDrag);
  AutoCVar_Float CVAR_clear_g("clear_color.g",
                              "Framebuffer clear color's green component", 0.f,
                              CVarFlags::kEditFloatDrag);
  AutoCVar_Float CVAR_clear_b("clear_color.b",
                              "Framebuffer clear color's blue component", 0.f,
                              CVarFlags::kEditFloatDrag);

  const VkPhysicalDeviceProperties& props = physical_device_.GetProperties();
  AutoCVar_String CVar_device_type(
      "device_type", "Device type",
      string_VkPhysicalDeviceType(props.deviceType), CVarFlags::kEditReadOnly);
  AutoCVar_String CVar_device_name("device_name", "Device name",
                                   props.deviceName, CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_push_constant_size(
      "limits.max_push_constant_size", "Max Push Constant Size",
      props.limits.maxPushConstantsSize, CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_memory_allocation_count(
      "limits.max_memory_allocation_count", "Max Memory Allocation Count",
      props.limits.maxMemoryAllocationCount, CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_bound_descriptor_sets(
      "limits.max_bound_descriptor_sets", "Max Bound Descriptor Sets",
      props.limits.maxBoundDescriptorSets, CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_descriptor_set_samplers(
      "limits.max_descriptor_set_samplers", "Max Descriptor Set Samplers",
      props.limits.maxDescriptorSetSamplers, CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_descriptor_set_uniform_buffers(
      "limits.max_descriptor_set_uniform_buffers",
      "Max Descriptor Set Uniform Buffers",
      props.limits.maxDescriptorSetUniformBuffers, CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_descriptor_set_dynamic_uniform_buffers(
      "limits.max_descriptor_set_dynamic_uniform_buffers",
      "Max Descriptor Set Dynamic Uniform Buffers",
      props.limits.maxDescriptorSetUniformBuffersDynamic,
      CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_descriptor_set_storage_buffers(
      "limits.max_descriptor_set_storage_buffers",
      "Max Descriptor Set Storage Buffers",
      props.limits.maxDescriptorSetStorageBuffers, CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_descriptor_set_dynamic_storage_buffers(
      "limits.max_descriptor_set_dynamic_storage_buffers",
      "Max Descriptor Set Dynamic Storage Buffers",
      props.limits.maxDescriptorSetStorageBuffersDynamic,
      CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_descriptor_set_sampled_images(
      "limits.max_descriptor_set_sampled_images",
      "Max Descriptor Set Sampled Images",
      props.limits.maxDescriptorSetSampledImages, CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_descriptor_set_storage_images(
      "limits.max_descriptor_set_storage_images",
      "Max Descriptor Set Storage Images",
      props.limits.maxDescriptorSetStorageImages, CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_descriptor_set_input_attachments(
      "limits.max_descriptor_set_input_attachments",
      "Max Descriptor Set Input Attachments",
      props.limits.maxDescriptorSetInputAttachments, CVarFlags::kEditReadOnly);
  AutoCVar_Int CVar_max_sample_count(
      "limits.max_sample_count", "Max Sample Count",
      physical_device_.GetMaxSamples(), CVarFlags::kEditReadOnly);
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
  }

  const size_t scene_data_size =
      kMaxFramesInFlight * PadUniformBuffer(sizeof(SceneData));
  scene_buffer_.Create(allocator_, scene_data_size,
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  Renderer::DescriptorSetLayout single_texture_layout;
  single_texture_layout.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   VK_SHADER_STAGE_FRAGMENT_BIT);
  single_texture_layout.Create(&device_);
  single_texture_set_layout_ = single_texture_layout.layout;

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    VkDescriptorBufferInfo scene_info{};
    scene_info.buffer = scene_buffer_.GetBuffer();
    scene_info.offset = 0;
    scene_info.range = sizeof(SceneData);

    VkDescriptorBufferInfo object_info{};
    object_info.buffer = frames_[i].object_buffer_.GetBuffer();
    object_info.offset = 0;
    object_info.range = sizeof(ObjectData) * kMaxObjects;

    Renderer::DescriptorBuilder::Begin(&layout_cache_, &descriptor_allocator_)
        .BindBuffer(0, &scene_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build(frames_[i].global_descriptor_, global_set_layout_);

    Renderer::DescriptorBuilder::Begin(&layout_cache_, &descriptor_allocator_)
        .BindBuffer(0, &object_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_SHADER_STAGE_VERTEX_BIT)
        .Build(frames_[i].object_descriptor_, object_set_layout_);
  }
}

void VulkanEngine::InitPipelines() {
  VkExtent2D extent = swapchain_.GetImageExtent();
  VkViewport viewport{0.f, 0.f, (float)extent.width, (float)extent.height,
                      0.f, 1.f};
  VkRect2D scissors{{0, 0}, extent};
  Renderer::PipelineBuilder builder(&device_);

  std::vector<VkDescriptorSetLayout> set_layouts;
  set_layouts.push_back(global_set_layout_);
  set_layouts.push_back(object_set_layout_);
  std::vector<VkPushConstantRange> push_constants;
  Renderer::Pipeline mesh_pipeline =
      builder.SetDefaults()
          .SetVertexShader("Shaders/tri_mesh.vert.spv")
          .SetFragmentShader("Shaders/default_lit.frag.spv")
          .SetVertexInputDescription(Renderer::Vertex::GetDescription())
          .SetRasterizer(VK_POLYGON_MODE_FILL, 1.f, VK_CULL_MODE_BACK_BIT,
                         VK_FRONT_FACE_COUNTER_CLOCKWISE)
          .SetDepthStencil()
          .SetLayout(set_layouts, push_constants)
          .SetViewport(viewport)
          .SetScissors(scissors)
          .SetMultisampling(physical_device_.GetMaxSamples(), VK_TRUE)
          .Build(&render_pass_);

  set_layouts.push_back(single_texture_set_layout_);
  Renderer::Pipeline texture_pipeline =
      builder.SetDefaults()
          .SetVertexShader("Shaders/tri_mesh.vert.spv")
          .SetFragmentShader("Shaders/textured_lit.frag.spv")
          .SetVertexInputDescription(Renderer::Vertex::GetDescription())
          .SetRasterizer(VK_POLYGON_MODE_FILL, 1.f, VK_CULL_MODE_BACK_BIT,
                         VK_FRONT_FACE_COUNTER_CLOCKWISE)
          .SetDepthStencil()
          .SetLayout(set_layouts, push_constants)
          .SetViewport(viewport)
          .SetScissors(scissors)
          .SetMultisampling(physical_device_.GetMaxSamples(), VK_TRUE)
          .Build(&render_pass_);

  CreateMaterial(mesh_pipeline, "default");
  CreateMaterial(texture_pipeline, "textured");
}

void VulkanEngine::LoadMeshes() {
  Renderer::CommandBuffer command_buffer = init_pool_.GetBuffer();
  command_buffer.Begin();

  Renderer::Mesh viking_room;
  viking_room.LoadFromAsset(allocator_, command_buffer,
                            "Assets/viking_room.mesh");

  command_buffer.End();
  command_buffer.AddToBatch();

  meshes_["room"] = viking_room;
}

void VulkanEngine::LoadTextures() {
  texture_sampler_.SetDefaults().Create(&device_);

  Renderer::CommandBuffer command_buffer = init_pool_.GetBuffer();
  command_buffer.Begin();

  Renderer::Texture viking_texture;
  viking_texture.LoadFromAsset(allocator_, &device_, command_buffer,
                               "Assets/viking_room.tx");

  command_buffer.End();
  command_buffer.AddToBatch();

  textures_["viking"] = viking_texture;
}

void VulkanEngine::InitScene() {
  Renderer::RenderObject room;
  room.Create(GetMesh("room"), GetMaterial("textured"));
  room.ModelMatrix() = glm::translate(glm::vec3(0, 0, -5));
  room.ModelMatrix() = glm::rotate(room.ModelMatrix(), glm::radians(-45.f),
                                   glm::vec3(1.f, 0.f, 0.f));
  room.ModelMatrix() = glm::rotate(room.ModelMatrix(), glm::radians(-135.f),
                                   glm::vec3(0.f, 0.f, 1.f));
  renderables_.push_back(room);

  Renderer::Material* viking_mat = GetMaterial("textured");
  VkDescriptorImageInfo viking_info{};
  viking_info.sampler = texture_sampler_.GetSampler();
  viking_info.imageView = textures_["viking"].GetView();
  viking_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  Renderer::DescriptorBuilder::Begin(&layout_cache_, &descriptor_allocator_)
      .BindImage(0, &viking_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
      .Build(viking_mat->texture_set);
}

void VulkanEngine::InitImgui() {
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_SAMPLER, 1000);
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    1000);
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000);
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000);
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                    1000);
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                    1000);
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000);
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000);
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                    1000);
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                                    1000);
  imgui_pool_.SetMaxDescriptorCount(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000);
  imgui_pool_.Create(&device_, 1000);

  ImGui::CreateContext();

  ImGui_ImplGlfw_InitForVulkan(window_.GetWindow(), true);

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = instance_.GetInstance();
  init_info.PhysicalDevice = physical_device_.GetDevice();
  init_info.Device = device_.GetDevice();
  init_info.Queue = device_.GetGraphicsQueue().GetQueue();
  init_info.DescriptorPool = imgui_pool_.GetPool();
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.MSAASamples = physical_device_.GetMaxSamples();

  ImGui_ImplVulkan_Init(&init_info, render_pass_.GetRenderPass());

  Renderer::CommandBuffer command_buffer = init_pool_.GetBuffer();
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

Renderer::Material* VulkanEngine::CreateMaterial(Renderer::Pipeline pipeline,
                                                 const std::string& name) {
  Renderer::Material mat{VK_NULL_HANDLE, pipeline};
  materials_[name] = mat;
  return &materials_[name];
}

Renderer::Material* VulkanEngine::GetMaterial(const std::string& name) {
  auto iter = materials_.find(name);
  if (iter == materials_.end()) return nullptr;
  return &(*iter).second;
}

Renderer::Mesh* VulkanEngine::GetMesh(const std::string& name) {
  auto iter = meshes_.find(name);
  if (iter == meshes_.end()) return nullptr;
  return &(*iter).second;
}

size_t VulkanEngine::PadUniformBuffer(size_t size) {
  size_t min_ubo_alignment =
      physical_device_.GetProperties().limits.minUniformBufferOffsetAlignment;
  size_t aligned_size = size;
  if (min_ubo_alignment > 0) {
    aligned_size =
        (aligned_size + min_ubo_alignment - 1) & ~(min_ubo_alignment - 1);
  }
  return aligned_size;
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
        if (menu_opened_) EnableCursor(true);
        break;
      case GLFW_KEY_F1:
        console_opened_ = !console_opened_;
        if (console_opened_) EnableCursor(true);
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
  if (menu_opened_ || console_opened_) return;

  if (!cursor_enabled_ &&
      glfwGetKey(window_.GetWindow(), GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
    EnableCursor(true);
  } else if (cursor_enabled_ && glfwGetKey(window_.GetWindow(),
                                           GLFW_KEY_LEFT_ALT) == GLFW_RELEASE) {
    EnableCursor(false);
  }

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
    imgui_pool_.Destroy();
    ImGui_ImplVulkan_Shutdown();

    for (auto& texture : textures_) texture.second.Destroy();
    for (auto& mesh : meshes_) mesh.second.Destroy();
    for (auto& material : materials_) material.second.Destroy();
    texture_sampler_.Destroy();

    scene_buffer_.Destroy();
    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
      vkDestroySemaphore(device_.GetDevice(), frames_[i].render_semaphore_,
                         nullptr);
      vkDestroySemaphore(device_.GetDevice(), frames_[i].present_semaphore_,
                         nullptr);
      vkDestroyFence(device_.GetDevice(), frames_[i].render_fence_, nullptr);

      frames_[i].command_pool_.Destroy();
      frames_[i].object_buffer_.Destroy();
    }

    vkDestroyDescriptorSetLayout(device_.GetDevice(),
                                 single_texture_set_layout_, nullptr);

    layout_cache_.Destroy();
    descriptor_allocator_.Destroy();

    swapchain_framebuffers_.Destroy();
    render_pass_.Destroy();

    depth_image_.Destroy();
    color_image_.Destroy();

    swapchain_.Destroy();

    vmaDestroyAllocator(allocator_);

    Logger::Cleanup();

    device_.Destroy();
    surface_.Destroy();
    instance_.Destroy();

    window_.Destroy();
  }
}

void VulkanEngine::Draw() {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;

  VK_CHECK(vkWaitForFences(device_.GetDevice(), 1,
                           &frames_[frame_index].render_fence_, VK_TRUE,
                           UINT64_MAX));

  uint32_t image_index;
  VkResult res = swapchain_.AcquireNextImage(
      &image_index, frames_[frame_index].present_semaphore_);

  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapchain();
    return;
  } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
    VK_CHECK(res);
  }

  VK_CHECK(vkResetFences(device_.GetDevice(), 1,
                         &frames_[frame_index].render_fence_));

  VK_CHECK(frames_[frame_index].command_pool_.Reset());

  ImGui::Render();

  Renderer::CommandBuffer command_buffer =
      frames_[frame_index].command_pool_.GetBuffer();
  VK_CHECK(command_buffer.Begin());

  VkClearValue clear_value{};
  clear_value.color = {{*CVarSystem::Get()->GetFloatCVar("clear_color.r"),
                        *CVarSystem::Get()->GetFloatCVar("clear_color.g"),
                        *CVarSystem::Get()->GetFloatCVar("clear_color.b"),
                        1.f}};
  VkClearValue depth_clear{};
  depth_clear.depthStencil.depth = 1.f;

  render_pass_.Begin(command_buffer,
                     swapchain_framebuffers_.GetFramebuffer(image_index),
                     {{0, 0}, swapchain_.GetImageExtent()},
                     {clear_value, depth_clear, clear_value});

  DrawObjects(command_buffer, renderables_.data(), renderables_.size());

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                  command_buffer.GetBuffer());

  render_pass_.End(command_buffer);

  VK_CHECK(command_buffer.End());

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &frames_[frame_index].present_semaphore_;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &frames_[frame_index].render_semaphore_;
  command_buffer.Submit(std::move(submit_info));

  VK_CHECK(device_.GetGraphicsQueue().SubmitBatches(
      frames_[frame_index].render_fence_));

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  VkSwapchainKHR swapchains[] = {swapchain_.GetSwapchain()};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &frames_[frame_index].render_semaphore_;
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

  glm::mat4 projection =
      glm::perspective(glm::radians(90.f), 1600.f / 900.f, 0.1f, 100.f);
  projection[1][1] *= -1;

  scene_data_.camera_data.view = camera_.GetViewMat();
  scene_data_.camera_data.projection = projection;

  float framed = frame_number_ / 120.f;
  scene_data_.ambient_color = {sin(framed), 0, cos(framed), 1};

  uint32_t uniform_offset =
      static_cast<uint32_t>(PadUniformBuffer(sizeof(SceneData))) * frame_index;
  scene_buffer_.SetData(&scene_data_, sizeof(SceneData), uniform_offset);

  ObjectData* object_SSBO = static_cast<ObjectData*>(
      frames_[frame_index].object_buffer_.GetMappedMemory());
  for (size_t i = 0; i < count; ++i) {
    Renderer::RenderObject& object = first[i];
    object_SSBO[i].model_matrix = object.ModelMatrix();
  }

  Renderer::Mesh* last_mesh = nullptr;
  Renderer::Material* last_material = nullptr;
  for (size_t i = 0; i < count; ++i) {
    Renderer::RenderObject& object = first[i];

    if (object.GetMaterial() != last_material) {
      object.GetMaterial()->pipeline.Bind(command_buffer);
      last_material = object.GetMaterial();
      vkCmdBindDescriptorSets(
          command_buffer.GetBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
          object.GetMaterial()->pipeline.GetLayout(), 0, 1,
          &frames_[frame_index].global_descriptor_, 1, &uniform_offset);
      vkCmdBindDescriptorSets(
          command_buffer.GetBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
          object.GetMaterial()->pipeline.GetLayout(), 1, 1,
          &frames_[frame_index].object_descriptor_, 0, nullptr);
      if (object.GetMaterial()->texture_set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            command_buffer.GetBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
            object.GetMaterial()->pipeline.GetLayout(), 2, 1,
            &object.GetMaterial()->texture_set, 0, nullptr);
      }
    }

    if (object.GetMesh() != last_mesh) {
      object.GetMesh()->BindBuffers(command_buffer);
      last_mesh = object.GetMesh();
    }

    vkCmdDrawIndexed(command_buffer.GetBuffer(),
                     object.GetMesh()->GetIndicesCount(), 1, 0, 0, 0);
  }
}

void VulkanEngine::DrawMenu() {
  ImGui::SetNextWindowSize(ImVec2{100.f, 0.f});
  VkExtent2D window_extent = window_.GetFramebufferSize();
  ImGui::SetNextWindowPos(ImVec2{static_cast<float>(window_extent.width) / 2,
                                 static_cast<float>(window_extent.height) / 2},
                          ImGuiCond_Always, ImVec2{0.5f, 0.5f});
  ImGui::Begin("Menu", &menu_opened_,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
  ImVec2 region = ImGui::GetContentRegionAvail();
  if (ImGui::Button("Exit", ImVec2{region.x, 0.f})) {
    window_.Close();
  }

  ImGui::End();
}

void VulkanEngine::DrawToolbar() {
  const ImVec2 button_size{50.f, 50.f};
  ImGui::SetNextWindowSize(ImVec2{0.f, 0.f});
  ImGui::SetNextWindowPos(ImVec2{0.f, 0.f});
  ImGui::Begin("Toolbar", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoMove);

  if (ImGui::Button("Debug\n(F1)", button_size))
    console_opened_ = !console_opened_;

  ImGui::End();
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
    if (console_opened_) CVarSystem::Get()->DrawImguiEditor();

    Draw();
  }

  device_.WaitIdle();
}

}  // namespace Engine