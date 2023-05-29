#include "VulkanEngine.h"

#include <iostream>

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

#define VK_CHECK(x)                                                       \
  do {                                                                    \
    VkResult err = x;                                                     \
    if (err) {                                                            \
      std::cerr << "Vulkan error: " << string_VkResult(err) << std::endl; \
      abort();                                                            \
    }                                                                     \
  } while (0);

#ifdef NDEBUG
const bool kEnableValidationLayers = false;
#else
const bool kEnableValidationLayers = true;
#endif

VulkanEngine::VulkanEngine() : is_initialized_(false), frame_number_(0) {}

VulkanEngine::~VulkanEngine() {}

void VulkanEngine::Init() {
  window_.Init(1600, 900, "Vulkan Engine");
  VK_CHECK(
      instance_.Init(kEnableValidationLayers, {"VK_LAYER_KHRONOS_validation"}));
  VK_CHECK(surface_.Init(&instance_, &window_));
  VK_CHECK(
      physical_device_.Init(&instance_, &surface_,
                            {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                             VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME}));
  VK_CHECK(device_.Init(&physical_device_));

  VmaAllocatorCreateInfo allocator_info{};
  allocator_info.instance = instance_.GetInstance();
  allocator_info.physicalDevice = physical_device_.GetDevice();
  allocator_info.device = device_.GetDevice();
  allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;

  VK_CHECK(vmaCreateAllocator(&allocator_info, &allocator_));

  VK_CHECK(swapchain_.Create(&device_, &surface_));

  VkExtent2D extent = swapchain_.GetImageExtent();
  VK_CHECK(depth_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_FORMAT_D32_SFLOAT,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT));

  VK_CHECK(render_pass_.CreateDefault(&device_, swapchain_.GetImageFormat()));
  VK_CHECK(swapchain_framebuffers_.Create(&swapchain_, &render_pass_, &depth_image_));

  for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    VK_CHECK(frames_[i].command_pool_.Create(
        &device_, device_.GetQueueFamilies().graphics_family.value()));

  InitSyncStructures();
  InitDescriptors();
  InitPipelines();

  LoadMeshes();
  LoadTextures();

  InitScene();

  InitImgui();

  device_.WaitIdle();
  is_initialized_ = true;
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
          .Build(&render_pass_);

  CreateMaterial(mesh_pipeline, "default");
  CreateMaterial(texture_pipeline, "textured");
}

void VulkanEngine::LoadMeshes() {
  Renderer::CommandBuffer command_buffer = frames_[0].command_pool_.GetBuffer();
  command_buffer.Begin(true);

  Renderer::Mesh lost_empire;
  lost_empire.LoadFromAsset(allocator_, command_buffer, "Assets/lost_empire.mesh");

  command_buffer.End();
  command_buffer.Submit();

  device_.GetGraphicsQueue().SubmitBatches();

  meshes_["empire"] = lost_empire;
}

void VulkanEngine::LoadTextures() {
  texture_sampler_.SetDefaults().Create(&device_);

  Renderer::CommandBuffer command_buffer = frames_[0].command_pool_.GetBuffer();
  command_buffer.Begin(true);

  Renderer::Texture texture;
  texture.LoadFromAsset(allocator_, &device_, command_buffer,
                        "Assets/lost_empire-RGBA.tx");

  command_buffer.End();
  command_buffer.Submit();

  device_.GetGraphicsQueue().SubmitBatches();

  textures_["empire_diffuse"] = texture;
}

void VulkanEngine::InitScene() {
  Renderer::RenderObject map;
  map.Create(GetMesh("empire"), GetMaterial("textured"));
  map.ModelMatrix() = glm::translate(glm::vec3(5, -15, 0));
  renderables_.push_back(map);

  Renderer::Material* textured_mat = GetMaterial("textured");

  VkDescriptorImageInfo image_info{};
  image_info.sampler = texture_sampler_.GetSampler();
  image_info.imageView = textures_["empire_diffuse"].GetView();
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  Renderer::DescriptorBuilder::Begin(&layout_cache_, &descriptor_allocator_)
      .BindImage(0, &image_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
      .Build(textured_mat->texture_set);
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
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info, render_pass_.GetRenderPass());

  Renderer::CommandBuffer command_buffer = frames_[0].command_pool_.GetBuffer();
  command_buffer.Begin();
  ImGui_ImplVulkan_CreateFontsTexture(command_buffer.GetBuffer());
  command_buffer.End();
  command_buffer.Submit();
  device_.GetGraphicsQueue().SubmitBatches();
  device_.GetGraphicsQueue().WaitIdle();

  ImGui_ImplVulkan_DestroyFontUploadObjects();
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

    swapchain_.Destroy();

    vmaDestroyAllocator(allocator_);

    device_.Destroy();
    surface_.Destroy();
    instance_.Destroy();

    window_.Destroy();
  }
}

void VulkanEngine::Draw() {
  VK_CHECK(vkWaitForFences(
      device_.GetDevice(), 1,
      &frames_[frame_number_ % kMaxFramesInFlight].render_fence_, VK_TRUE,
      UINT64_MAX));
  VK_CHECK(vkResetFences(
      device_.GetDevice(), 1,
      &frames_[frame_number_ % kMaxFramesInFlight].render_fence_));

  uint32_t image_index;
  VK_CHECK(swapchain_.AcquireNextImage(
      &image_index,
      frames_[frame_number_ % kMaxFramesInFlight].present_semaphore_));

  VK_CHECK(frames_[frame_number_ % kMaxFramesInFlight].command_pool_.Reset());

  ImGui::Render();

  Renderer::CommandBuffer command_buffer =
      frames_[frame_number_ % kMaxFramesInFlight].command_pool_.GetBuffer();
  VK_CHECK(command_buffer.Begin(true));

  VkClearValue clear_value{};
  float flash = abs(sin(frame_number_ / 120.f));
  clear_value.color = {{0.f, 0.f, flash, 1.f}};
  VkClearValue depth_clear{};
  depth_clear.depthStencil.depth = 1.f;

  render_pass_.Begin(command_buffer,
                     swapchain_framebuffers_.GetFramebuffer(image_index),
                     {{0, 0}, swapchain_.GetImageExtent()}, {clear_value, depth_clear});

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
  submit_info.pWaitSemaphores =
      &frames_[frame_number_ % kMaxFramesInFlight].present_semaphore_;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores =
      &frames_[frame_number_ % kMaxFramesInFlight].render_semaphore_;
  command_buffer.Submit(std::move(submit_info));

  VK_CHECK(device_.GetGraphicsQueue().SubmitBatches(
      frames_[frame_number_ % kMaxFramesInFlight].render_fence_));

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  VkSwapchainKHR swapchains[] = {swapchain_.GetSwapchain()};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores =
      &frames_[frame_number_ % kMaxFramesInFlight].render_semaphore_;
  present_info.pImageIndices = &image_index;

  VK_CHECK(device_.GetPresentQueue().Present(&present_info));

  frame_number_++;
}

void VulkanEngine::DrawObjects(Renderer::CommandBuffer command_buffer,
                               Renderer::RenderObject* first, size_t count) {
  glm::vec3 cam_pos{0.f, 0.f, -10.f};
  glm::mat4 view = glm::translate(glm::mat4{1.f}, cam_pos);
  glm::mat4 projection =
      glm::perspective(glm::radians(70.f), 1600.f / 900.f, 0.1f, 100.f);
  projection[1][1] *= -1;

  scene_data_.camera_data.view = view;
  scene_data_.camera_data.projection = projection;

  float framed = frame_number_ / 120.f;
  scene_data_.ambient_color = {sin(framed), 0, cos(framed), 1};

  uint32_t uniform_offset = PadUniformBuffer(sizeof(SceneData)) *
                            (frame_number_ % kMaxFramesInFlight);
  scene_buffer_.SetData(&scene_data_, sizeof(SceneData), uniform_offset);

  ObjectData* object_SSBO =
      static_cast<ObjectData*>(frames_[frame_number_ % kMaxFramesInFlight]
                                   .object_buffer_.GetMappedMemory());
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
          &frames_[frame_number_ % kMaxFramesInFlight].global_descriptor_, 1,
          &uniform_offset);
      vkCmdBindDescriptorSets(
          command_buffer.GetBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
          object.GetMaterial()->pipeline.GetLayout(), 1, 1,
          &frames_[frame_number_ % kMaxFramesInFlight].object_descriptor_, 0,
          nullptr);
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

    vkCmdDraw(command_buffer.GetBuffer(), object.GetMesh()->GetVerticesCount(),
              1, 0, i);
  }
}

void VulkanEngine::Run() {
  while (!window_.ShouldClose()) {
    window_.PollEvents();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    Draw();
  }

  device_.WaitIdle();
}
