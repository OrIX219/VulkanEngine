#include "VulkanEngine.h"

#include <iostream>
#include <functional>
#include <future>

#include <vulkan/vulkan.hpp>
#include <vulkan/vk_enum_string_helper.h>

#define VMA_IMPLEMENTATION
#include <vma\include\vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#include <glm/gtx/transform.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "Console/CVAR.h"
#include "Logger.h"
#include "PipelineBarriers.h"

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
      menu_opened_(false),
      samples_(VK_SAMPLE_COUNT_1_BIT) {}

VulkanEngine::~VulkanEngine() {}

void VulkanEngine::Init() {
  Logger::Get().SetTime();
  LOG_INFO("Initializing engine...");

  window_.Init(1600, 900, "Vulkan Engine", this);
  glfwSetInputMode(window_.GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  LOG_SUCCESS("Created window");
  VK_CHECK(instance_.Init(
      kEnableValidationLayers,
      {"VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_monitor"}));
  LOG_SUCCESS("Initialized Vulkan instance");

  Logger::Init(&instance_);
  LOG_SUCCESS("Initialized logger");

  VK_CHECK(surface_.Init(&instance_, &window_));
  LOG_SUCCESS("Initialized GLFW surface");
  VK_CHECK(
      physical_device_.Init(&instance_, &surface_,
                            {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                             VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
                             VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
                             VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME}));
  LOG_SUCCESS("Found GPU");
  samples_ = physical_device_.GetMaxSamples();
  VK_CHECK(device_.Init(&physical_device_));
  LOG_SUCCESS("Initialized logical device");

  profiler_.Init(&device_,
                 physical_device_.GetProperties().limits.timestampPeriod);
  LOG_SUCCESS("Initialized profiler");
  InitCVars();
  LOG_SUCCESS("Initialized CVar system");

  VmaAllocatorCreateInfo allocator_info{};
  allocator_info.instance = instance_.Get();
  allocator_info.physicalDevice = physical_device_.Get();
  allocator_info.device = device_.Get();
  allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;

  VK_CHECK(vmaCreateAllocator(&allocator_info, &allocator_));

  Renderer::CommandPool init_pool;
  VK_CHECK(init_pool.Create(
      &device_, device_.GetQueueFamilies().graphics_family.value()));

  VK_CHECK(swapchain_.Create(&device_, &surface_));
  LOG_SUCCESS("Created swapchain");

  color_attachment_format_ = VK_FORMAT_R16G16B16A16_SFLOAT;
  depth_attachment_format_ = VK_FORMAT_D32_SFLOAT;
  VkExtent2D extent = swapchain_.GetImageExtent();
  VK_CHECK(color_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      color_attachment_format_, VK_IMAGE_ASPECT_COLOR_BIT, samples_));
  LOG_SUCCESS("Created backbuffer image");
  VK_CHECK(color_resolve_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      color_attachment_format_, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_SAMPLE_COUNT_1_BIT));
  LOG_SUCCESS("Created backbuffer resolve image");
  VK_CHECK(depth_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
          VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
      depth_attachment_format_, VK_IMAGE_ASPECT_DEPTH_BIT, samples_));
  LOG_SUCCESS("Created depth image");
  VK_CHECK(depth_resolve_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      depth_attachment_format_, VK_IMAGE_ASPECT_DEPTH_BIT,
      VK_SAMPLE_COUNT_1_BIT));
  LOG_SUCCESS("Created depth resolve image");

  VK_CHECK(shadow_image_.Create(
      allocator_, &device_, {shadow_extent_.width, shadow_extent_.height, 1},
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      depth_attachment_format_, VK_IMAGE_ASPECT_DEPTH_BIT,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY, Renderer::kMaxDirectionalLights));
  LOG_SUCCESS("Created shadow image");
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::Image::Destroy, shadow_image_));

  VK_CHECK(point_shadow_image_.Create(
      allocator_, &device_, {128, 128, 1},
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      depth_attachment_format_, VK_IMAGE_ASPECT_DEPTH_BIT,
      Renderer::kMaxPointLights));
  LOG_SUCCESS("Created point shadow image");
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::Image::Destroy, point_shadow_image_));

  InitRenderPasses(samples_);
  LOG_SUCCESS("Initialized render passes");
  InitFramebuffers();
  LOG_SUCCESS("Initialized framebuffers");

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    VK_CHECK(frames_[i].command_pool.Create(
        &device_, device_.GetQueueFamilies().graphics_family.value()));
    main_deletion_queue_.PushFunction(
        std::bind(&Renderer::CommandPool::Destroy, frames_[i].command_pool));
    frames_[i].dynamic_descriptor_allocator.Init(&device_);
    main_deletion_queue_.PushFunction(
        std::bind(&Renderer::DescriptorAllocator::Destroy,
                  frames_[i].dynamic_descriptor_allocator));
  }

  VK_CHECK(upload_pool_.Create(
      &device_, device_.GetQueueFamilies().transfer_family.value()));
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::CommandPool::Destroy, upload_pool_));

  shader_cache_.Init(&device_);

  render_scene_.Init();

  Renderer::MaterialSystem::Init(this);
  LOG_SUCCESS("Initialized material system");
  
  InitSyncStructures();
  InitDescriptors();
  LOG_SUCCESS("Initialized descriptors");
  InitPipelines();
  LOG_SUCCESS("Initialized pipelines");
  InitSamplers();
  LOG_SUCCESS("Initialized samplers");
  InitDepthPyramid(init_pool);
  LOG_SUCCESS("Created depth pyramid");

  Renderer::Queue& init_queue = device_.GetQueue(init_pool.GetQueueFamily());

  init_queue.BeginBatch();
  InitScene(init_pool);
  init_queue.EndBatch();

  InitImgui(init_pool);

  render_scene_.MergeMeshes(this);
  render_scene_.BuildBatches();

  init_queue.SubmitBatches();
  device_.GetTransferQueue().SubmitBatches();  
  LOG_SUCCESS("Initialized scene");

  device_.WaitIdle();
  is_initialized_ = true;

  LOG_INFO("Finished initializing engine");

  init_pool.Destroy();
}

void VulkanEngine::Cleanup() {
  if (is_initialized_) {
    ImGui_ImplVulkan_Shutdown();

    main_deletion_queue_.Flush();
    for (FrameData& frame : frames_)
      frame.dynamic_descriptor_allocator.Destroy();

    Renderer::MaterialSystem::Cleanup();

    forward_framebuffer_.Destroy();
    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
      swapchain_framebuffers_[i].Destroy();
    depth_pyramid_.Destroy();
    depth_resolve_image_.Destroy();
    depth_image_.Destroy();
    color_resolve_image_.Destroy();
    color_image_.Destroy();
    swapchain_.Destroy();

    skybox_texture_.Destroy();
    render_scene_.Destroy();

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

void VulkanEngine::InitCVars() {
  AutoCVar_Int CVar_show_normals("show_normals", "Render vertex normals", 0,
                                 CVarFlagBits::kEditCheckbox);

  AutoCVar_Float CVar_hdr_exposure("hdr_exposure", "HDR exposure value", 1.f,
                                   CVarFlagBits::kEditFloatDrag);

  AutoCVar_Vec4 CVar_clear_color("scene.clear_color", "Background color",
                                 {0.f, 0.f, 0.f, 1.f},
                                 CVarFlagBits::kEditColor);
  AutoCVar_Vec3 CVar_sunlight_dir("scene.sunlight_dir", "Sunlight direction",
                                  {1.f, -1.f, 1.f});
  AutoCVar_Vec4 CVar_sunlight_color(
      "scene.sunlight_color", "Sunlight color (xyz) and power (w)",
      {1.f, 1.f, 1.f, 0.1f}, CVarFlagBits::kEditColor);

  AutoCVar_Int CVar_cull_enable("culling.enable", "Enable culling", 1,
                                CVarFlagBits::kEditCheckbox);
  AutoCVar_Int CVar_cull_occlusion_enable("culling.occlusion_culling",
                                          "Enable occlusion culling", 1,
                                          CVarFlagBits::kEditCheckbox);
  AutoCVar_Float CVar_cull_dist("culling.distance",
                                "Cull objects further than this", 1000.f,
                                CVarFlagBits::kEditFloatDrag);

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

void VulkanEngine::InitRenderPasses(VkSampleCountFlagBits samples) {
  Renderer::RenderPassBuilder render_pass_builder(&device_);
  {
    Renderer::RenderPassAttachment color_attachment, color_resolve_attachment,
        depth_attachment, depth_resolve_attachment;
    color_attachment
        .SetOperations(VK_ATTACHMENT_LOAD_OP_CLEAR,
                       VK_ATTACHMENT_STORE_OP_STORE)
        .SetSamples(samples)
        .SetLayouts(VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .SetFormat(color_attachment_format_);
    color_resolve_attachment
        .SetOperations(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                       VK_ATTACHMENT_STORE_OP_STORE)
        .SetSamples(VK_SAMPLE_COUNT_1_BIT)
        .SetLayouts(VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .SetFormat(color_attachment_format_);
    depth_attachment
        .SetOperations(VK_ATTACHMENT_LOAD_OP_CLEAR,
                       VK_ATTACHMENT_STORE_OP_STORE)
        .SetSamples(samples)
        .SetLayouts(VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .SetFormat(depth_attachment_format_);
    depth_resolve_attachment.SetDefaults()
        .SetOperations(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                       VK_ATTACHMENT_STORE_OP_STORE)
        .SetLayouts(VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .SetFormat(depth_attachment_format_);

    Renderer::RenderPassSubpass subpass;
    render_pass_builder.AddAttachment(color_attachment)
        .AddAttachment(depth_attachment)
        .AddAttachment(color_resolve_attachment)
        .AddAttachment(depth_resolve_attachment);
    subpass.AddColorAttachmentRef(0)
        .SetDepthStencilAttachmentRef(1)
        .AddResolveAttachmentRef(2)
        .SetDepthStencilResolveAttachmentRef(3);

    render_pass_builder.AddSubpass(subpass);
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

    forward_pass_ = render_pass_builder.Build();

    main_deletion_queue_.PushFunction(
        std::bind(&Renderer::RenderPass::Destroy, forward_pass_));
  }
  render_pass_builder.Clear();
  {
    Renderer::RenderPassAttachment color_attachment;
    color_attachment.SetDefaults()
        .SetOperations(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                       VK_ATTACHMENT_STORE_OP_STORE)
        .SetFormat(swapchain_.GetImageFormat());

    Renderer::RenderPassSubpass subpass;
    render_pass_builder.AddAttachment(color_attachment);
    subpass.AddColorAttachmentRef(0);

    render_pass_builder.AddSubpass(subpass);
    render_pass_builder.AddDependency(
        VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    copy_pass_ = render_pass_builder.Build();

    main_deletion_queue_.PushFunction(
        std::bind(&Renderer::RenderPass::Destroy, copy_pass_));
  }
  render_pass_builder.Clear();
  {
    Renderer::RenderPassAttachment depth_attachment;
    depth_attachment
        .SetOperations(VK_ATTACHMENT_LOAD_OP_CLEAR,
                       VK_ATTACHMENT_STORE_OP_STORE)
        .SetSamples(VK_SAMPLE_COUNT_1_BIT)
        .SetLayouts(VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .SetFormat(depth_attachment_format_);

    Renderer::RenderPassSubpass subpass;
    render_pass_builder.AddAttachment(depth_attachment);
    subpass.SetDepthStencilAttachmentRef(0);

    render_pass_builder.AddSubpass(subpass);
    render_pass_builder.AddDependency(
        VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    directional_shadow_pass_ = render_pass_builder.Build();

    main_deletion_queue_.PushFunction(
        std::bind(&Renderer::RenderPass::Destroy, directional_shadow_pass_));
  }
  render_pass_builder.Clear();
  {
    Renderer::RenderPassAttachment depth_attachment;
    depth_attachment
        .SetOperations(VK_ATTACHMENT_LOAD_OP_CLEAR,
                       VK_ATTACHMENT_STORE_OP_STORE)
        .SetSamples(VK_SAMPLE_COUNT_1_BIT)
        .SetLayouts(VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .SetFormat(depth_attachment_format_);

    Renderer::RenderPassSubpass subpass;
    render_pass_builder.AddAttachment(depth_attachment);
    subpass.SetDepthStencilAttachmentRef(0);

    render_pass_builder.AddSubpass(subpass);
    render_pass_builder.AddDependency(
        VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    point_shadow_pass_ = render_pass_builder.Build();

    main_deletion_queue_.PushFunction(
        std::bind(&Renderer::RenderPass::Destroy, point_shadow_pass_));
  }
}

void VulkanEngine::InitFramebuffers() {
  VkExtent2D extent = swapchain_.GetImageExtent();
  std::vector<VkImageView> attachments = {
      color_image_.GetView(), depth_image_.GetView(),
      color_resolve_image_.GetView(), depth_resolve_image_.GetView()};

  VK_CHECK(forward_framebuffer_.Create(&device_, &forward_pass_, extent,
                                       attachments));

  VK_CHECK(shadow_framebuffer_.Create(&device_, &directional_shadow_pass_,
                                      shadow_extent_,
                                      {shadow_image_.GetView()}));
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::Framebuffer::Destroy, shadow_framebuffer_));

  VkExtent3D point_extent = point_shadow_image_.GetExtent();
  VK_CHECK(point_shadow_framebuffer_.Create(
      &device_, &point_shadow_pass_, {point_extent.width, point_extent.height},
      {point_shadow_image_.GetView()}, point_shadow_image_.GetArrayLayers()));
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::Framebuffer::Destroy, point_shadow_framebuffer_));

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    VK_CHECK(swapchain_framebuffers_[i].Create(&device_, &copy_pass_, extent,
                                               {swapchain_.GetImageView(i)}));
  }
}

void VulkanEngine::InitSyncStructures() {
  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VK_CHECK(vkCreateFence(device_.Get(), &fence_info, nullptr,
                         &window_resize_fence_));
  main_deletion_queue_.PushFunction(
      [=]() { vkDestroyFence(device_.Get(), window_resize_fence_, nullptr);
  });
  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    VK_CHECK(vkCreateFence(device_.Get(), &fence_info, nullptr,
                           &frames_[i].render_fence));
    VK_CHECK(vkCreateSemaphore(device_.Get(), &semaphore_info, nullptr,
                               &frames_[i].render_semaphore));
    VK_CHECK(vkCreateSemaphore(device_.Get(), &semaphore_info, nullptr,
                               &frames_[i].present_semaphore));

    main_deletion_queue_.PushFunction([=]() {
      vkDestroyFence(device_.Get(), frames_[i].render_fence, nullptr);
      vkDestroySemaphore(device_.Get(), frames_[i].render_semaphore, nullptr);
      vkDestroySemaphore(device_.Get(), frames_[i].present_semaphore, nullptr);
    });
  }
}

void VulkanEngine::InitDescriptors() {
  descriptor_allocator_.Init(&device_);
  layout_cache_.Init(&device_);

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    frames_[i].dynamic_data.Create(
        allocator_, 64 * 1024,
        static_cast<uint32_t>(physical_device_.GetProperties()
                                  .limits.minUniformBufferOffsetAlignment));
    main_deletion_queue_.PushFunction(
        std::bind(&Renderer::PushBuffer::Destroy, frames_[i].dynamic_data));
  }
}

void VulkanEngine::InitPipelines() {
  Renderer::ShaderEffect* blit_effect = new Renderer::ShaderEffect();
  blit_effect->AddStage(shader_cache_.GetShader("Shaders/blit.vert.spv"),
                        VK_SHADER_STAGE_VERTEX_BIT);
  blit_effect->AddStage(shader_cache_.GetShader("Shaders/blit.frag.spv"),
                        VK_SHADER_STAGE_FRAGMENT_BIT);
  blit_effect->ReflectLayout(&device_);
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::ShaderEffect::Destroy, *blit_effect));

  Renderer::PipelineBuilder builder = Renderer::PipelineBuilder::Begin(&device_);
  blit_pipeline_ =
      builder.SetDefaults()
          .SetDepthStencil(VK_FALSE)
          .SetShaders(blit_effect)
          .Build(copy_pass_);
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::Pipeline::Destroy, blit_pipeline_));

  LoadComputeShader("Shaders/indirect_compute.comp.spv", cull_pipeline_,
                    cull_layout_);
  LoadComputeShader("Shaders/depth_reduce.comp.spv", depth_reduce_pipeline_,
                    depth_reduce_layout_);
  LoadComputeShader("Shaders/sparse_upload.comp.spv", sparse_upload_pipeline_,
                    sparse_upload_layout_);

  Renderer::MaterialData normals;
  normals.base_template = "normals";
  Renderer::Material* material =
      Renderer::MaterialSystem::BuildMaterial("normals", normals);
  if (!material) LOG_FATAL("Failed to build normals material");

  std::array<Renderer::ShaderEffect::ReflectionOverrides, 1> overrides = {
      {"sceneData", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC}};
  Renderer::ShaderEffect* axes_effect = new Renderer::ShaderEffect();
  axes_effect->AddStage(shader_cache_.GetShader("Shaders/axes.vert.spv"),
                        VK_SHADER_STAGE_VERTEX_BIT);
  axes_effect->AddStage(shader_cache_.GetShader("Shaders/axes.frag.spv"),
                        VK_SHADER_STAGE_FRAGMENT_BIT);
  axes_effect->AddStage(shader_cache_.GetShader("Shaders/axes.geom.spv"),
                        VK_SHADER_STAGE_GEOMETRY_BIT);
  axes_effect->ReflectLayout(&device_, overrides.data(), overrides.size());
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::ShaderEffect::Destroy, *axes_effect));

  builder = Renderer::PipelineBuilder::Begin(&device_);
  axes_pipeline_ =
      builder.SetDefaults()
          .SetShaders(axes_effect)
          .SetDepthStencil(VK_FALSE)
          .SetMultisampling(samples_)
          .SetVertexInputDescription(Renderer::Vertex::GetDescription())
          .SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
          .Build(forward_pass_);
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::Pipeline::Destroy, axes_pipeline_));
}

void VulkanEngine::InitSamplers() {
  texture_sampler_.SetDefaults().Create(&device_);
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::TextureSampler::Destroy, texture_sampler_));

  VkSamplerReductionModeCreateInfo reduction_info{};
  reduction_info.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
  reduction_info.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX;

  depth_reduction_sampler_.SetDefaults()
      .SetAddressMode({VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE})
      .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
      .Create(&device_, 0.f, 16.f, &reduction_info);
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::TextureSampler::Destroy, depth_reduction_sampler_));

  depth_sampler_.SetDefaults()
      .SetAddressMode({VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE})
      .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
      .Create(&device_, 0.f, 16.f, &reduction_info);
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::TextureSampler::Destroy, depth_sampler_));

  smooth_sampler_.SetDefaults().SetAnisotropyEnable(true).Create(&device_);
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::TextureSampler::Destroy, smooth_sampler_));

  skybox_sampler_.SetDefaults()
      .SetAddressMode({VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE})
      .Create(&device_);
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::TextureSampler::Destroy, skybox_sampler_));

  shadow_sampler_.SetDefaults()
      .SetAddressMode({VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER})
      .SetCompare(VK_TRUE, VK_COMPARE_OP_LESS)
      .SetBorderColor(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE)
      .Create(&device_);
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::TextureSampler::Destroy, shadow_sampler_));
}

uint32_t PrevPowOf2(uint32_t x) {
  if (x == 0) return 0;
  --x;
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  return x - (x >> 1);
}

void VulkanEngine::InitDepthPyramid(Renderer::CommandPool& init_pool) {
  VkExtent2D extent = swapchain_.GetImageExtent();
  depth_pyramid_width_ = PrevPowOf2(extent.width);
  depth_pyramid_height_ = PrevPowOf2(extent.height);
  depth_pyramid_levels_ = Renderer::Image::CalculateMipLevels(
      depth_pyramid_width_, depth_pyramid_height_);

  VkExtent3D pyramid_extent = {depth_pyramid_width_, depth_pyramid_height_, 1};
  depth_pyramid_.Create(
      allocator_, &device_, pyramid_extent,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      depth_pyramid_levels_, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32_SFLOAT);

  for (int32_t i = 0; i < depth_pyramid_levels_; ++i) {
    VkImageViewCreateInfo level_info{};
    level_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    level_info.format = VK_FORMAT_R32_SFLOAT;
    level_info.image = depth_pyramid_.Get();
    level_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    level_info.subresourceRange.baseMipLevel = i;
    level_info.subresourceRange.levelCount = 1;
    level_info.subresourceRange.layerCount = 1;
    level_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

    VkImageView view;
    vkCreateImageView(device_.Get(), &level_info, nullptr, &view);
    depth_pyramid_mips_[i] = view;
    main_deletion_queue_.PushFunction(
        [=]() { vkDestroyImageView(device_.Get(), view, nullptr); });
  }

  Renderer::CommandBuffer command_buffer = init_pool.GetBuffer();
  command_buffer.Begin(true);
  Renderer::Image::LayoutTransitionInfo layout_info{};
  layout_info.src_access = VK_ACCESS_SHADER_WRITE_BIT;
  layout_info.dst_access = VK_ACCESS_SHADER_READ_BIT;
  layout_info.new_layout = VK_IMAGE_LAYOUT_GENERAL;
  layout_info.src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  layout_info.dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  layout_info.dependency = VK_DEPENDENCY_BY_REGION_BIT;
  depth_pyramid_.LayoutTransition(command_buffer, layout_info);
  command_buffer.End();
  command_buffer.Submit();
}

bool VulkanEngine::LoadComputeShader(const char* path, VkPipeline& pipeline,
                                     VkPipelineLayout& layout) {
  Renderer::ShaderModule compute_module;
  if (!Renderer::LoadShaderModule(&device_, path, &compute_module)) {
    LOG_ERROR("Failed to load compute shader from '{}'", path);
    return false;
  }

  Renderer::ShaderEffect* effect = new Renderer::ShaderEffect();
  effect->AddStage(&compute_module, VK_SHADER_STAGE_COMPUTE_BIT);
  effect->ReflectLayout(&device_);

  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::ShaderEffect::Destroy, effect));

  Renderer::ComputePipelineBuilder builder =
      Renderer::ComputePipelineBuilder::Begin(&device_);
  builder.SetLayout(effect->built_layout);
  VkPipelineShaderStageCreateInfo stage{};
  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.module = compute_module.module;
  stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage.pName = "main";
  builder.SetShaderStage(stage);

  pipeline = builder.Build();
  layout = effect->built_layout;

  vkDestroyShaderModule(device_.Get(), compute_module.module, nullptr);

  main_deletion_queue_.PushFunction(
      [=]() { vkDestroyPipeline(device_.Get(), pipeline, nullptr);
  });

  return true;
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

  std::vector<Renderer::RenderObject> prefab_renderables;
  prefab_renderables.reserve(info->node_meshes.size());

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
      tex.sampler = texture_sampler_.Get();
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

    object.draw_forward_pass = true;
    object.draw_shadow_pass = true;

    glm::mat4 node_matrix{1.f};

    auto matrix_iter = node_world_mats.find(key);
    if (matrix_iter != node_world_mats.end()) {
      memcpy(&node_matrix, &(matrix_iter->second), sizeof(glm::mat4));
    }

    object.Create(GetMesh(value.mesh_path), material);
    object.model_mat = node_matrix;
    object.RefreshRenderBounds();

    prefab_renderables.push_back(object);
  }

  render_scene_.RegisterObjectBatch(
      prefab_renderables.data(),
      static_cast<uint32_t>(prefab_renderables.size()));

  return true;
}

void VulkanEngine::InitScene(Renderer::CommandPool& init_pool) {
  /*Renderer::DirectionalLight sunlight(
      glm::vec4(1.f), glm::vec3(0.f),
      *CVarSystem::Get()->GetVec3CVar("scene.sunlight_dir"),
      glm::vec3(32, 32, 32));
  directional_lights_.emplace_back(std::move(sunlight));*/

  Renderer::PointLight point_light(glm::vec4(1.f, 0.75f, 0.25f, 1.f),
                                   glm::vec3(0.f, 1.f, -3.5f), 1.f, 0.09f, 0.032f);
  point_light.SetDiffuse(10.f);
  point_light.SetSpecular(10.f);
  point_lights_.emplace_back(std::move(point_light));

  Renderer::CommandBuffer command_buffer = init_pool.GetBuffer();
  command_buffer.Begin();

  axes_buffer_.Create(allocator_, sizeof(Renderer::Vertex));
  axes_buffer_.SetData(
      command_buffer,
      {{{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {0.f, 0.f}}});
  main_deletion_queue_.PushFunction(
      std::bind(&Renderer::VertexBuffer::Destroy, axes_buffer_));

  LoadTexture(command_buffer, "white", AssetPath("white.tx").c_str());
  skybox_texture_.LoadFromDirectory(allocator_, &device_, command_buffer,
                                    AssetPath("skybox").c_str());

  Renderer::MaterialData wireframe_info;
  wireframe_info.base_template = "default_wireframe";
  Renderer::MaterialSystem::BuildMaterial("wireframe", wireframe_info);

  Renderer::SampledTexture white_texture;
  white_texture.sampler = texture_sampler_.Get();
  white_texture.view = textures_["white"].GetView();

  Renderer::MaterialData material_info;
  material_info.base_template = "texturedPBR_opaque";
  material_info.textures.push_back(white_texture);
  Renderer::MaterialSystem::BuildMaterial("default", material_info);

  Renderer::MaterialData textured_info;
  textured_info.base_template = "texturedPBR_opaque";
  textured_info.textures.push_back(white_texture);
  Renderer::MaterialSystem::BuildMaterial("textured", textured_info);

  Renderer::SampledTexture skybox_texture;
  skybox_texture.sampler = skybox_sampler_.Get();
  skybox_texture.view = skybox_texture_.GetView();

  Renderer::MaterialData skybox_info;
  skybox_info.base_template = "skybox";
  skybox_info.textures.push_back(skybox_texture);
  Renderer::MaterialSystem::BuildMaterial("skybox", skybox_info);

  LoadMesh(command_buffer, "cube", AssetPath("cube.mesh").c_str());

  LoadPrefab(command_buffer, AssetPath("Test.pfb").c_str());

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

  VK_CHECK(
      vkCreateDescriptorPool(device_.Get(), &pool_info, nullptr, &imgui_pool_));
  main_deletion_queue_.PushFunction(
      [=]() { vkDestroyDescriptorPool(device_.Get(), imgui_pool_, nullptr);
  });

  ImGui::CreateContext();

  ImGui_ImplGlfw_InitForVulkan(window_.GetWindow(), true);

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = instance_.Get();
  init_info.PhysicalDevice = physical_device_.Get();
  init_info.Device = device_.Get();
  init_info.Queue = device_.GetGraphicsQueue().Get();
  init_info.DescriptorPool = imgui_pool_;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info, copy_pass_.Get());

  Renderer::CommandBuffer command_buffer = init_pool.GetBuffer();
  command_buffer.Begin();
  ImGui_ImplVulkan_CreateFontsTexture(command_buffer.Get());
  command_buffer.End();
  command_buffer.Submit();
  device_.GetGraphicsQueue().SubmitBatches();
  device_.GetGraphicsQueue().WaitIdle();

  ImGui_ImplVulkan_DestroyFontUploadObjects();
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

VmaAllocator VulkanEngine::GetAllocator() { return allocator_; }

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

void VulkanEngine::RecreateSwapchain(Renderer::CommandPool& command_pool) {
  VkExtent2D extent = window_.GetFramebufferSize();
  if (extent.width == 0 || extent.height == 0) {
    extent = window_.GetFramebufferSize();
    window_.WaitEvents();
  }

  device_.WaitIdle();

  swapchain_.Recreate();

  extent = swapchain_.GetImageExtent();
  color_image_.Destroy();
  VK_CHECK(color_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      color_attachment_format_, VK_IMAGE_ASPECT_COLOR_BIT, samples_));

  color_resolve_image_.Destroy();
  VK_CHECK(color_resolve_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      color_attachment_format_, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_SAMPLE_COUNT_1_BIT));

  depth_image_.Destroy();
  VK_CHECK(depth_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
          VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
      depth_attachment_format_, VK_IMAGE_ASPECT_DEPTH_BIT, samples_));

  depth_resolve_image_.Destroy();
  VK_CHECK(depth_resolve_image_.Create(
      allocator_, &device_, {extent.width, extent.height, 1},
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      depth_attachment_format_, VK_IMAGE_ASPECT_DEPTH_BIT,
      VK_SAMPLE_COUNT_1_BIT));

  std::vector<VkImageView> attachments = {
      color_image_.GetView(), depth_image_.GetView(),
      color_resolve_image_.GetView(), depth_resolve_image_.GetView()};
  VK_CHECK(forward_framebuffer_.Resize(extent, attachments));
  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    VK_CHECK(swapchain_framebuffers_[i].Resize(extent,
                                               {swapchain_.GetImageView(i)}));
  }

  depth_pyramid_.Destroy();
  InitDepthPyramid(command_pool);
}

void VulkanEngine::Draw() {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  std::array<VkFence, 2> fences = {frame.render_fence, window_resize_fence_};
  VK_CHECK(vkWaitForFences(device_.Get(), static_cast<uint32_t>(fences.size()),
                           fences.data(), VK_TRUE, UINT64_MAX));

  uint32_t image_index;
  VkResult res =
      swapchain_.AcquireNextImage(&image_index, frame.present_semaphore);

  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapchain(frame.command_pool);
    return;
  } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
    VK_CHECK(res);
  }

  VK_CHECK(vkResetFences(device_.Get(), 1, &frame.render_fence));

  frame.deletion_queue.Flush();
  VK_CHECK(frame.command_pool.Reset());
  frame.dynamic_data.Reset();
  frame.dynamic_descriptor_allocator.ResetPools();

  ImGui::Render();

  Renderer::CommandBuffer command_buffer = frame.command_pool.GetBuffer();
  VK_CHECK(command_buffer.Begin());

  while (!prefabs_to_load_.empty()) {
    LoadPrefab(command_buffer, AssetPath(prefabs_to_load_.back()).c_str(),
               glm::mat4{1.f});
    prefabs_to_load_.pop_back();
    render_scene_.BuildBatches();
  }

  profiler_.GrabQueries(command_buffer);
  {
    Renderer::VulkanScopeTimer timer(command_buffer, &profiler_, "Full Frame");

    pre_cull_barriers_.clear();
    post_cull_barriers_.clear();

    {
      Renderer::VulkanScopeTimer timer2(command_buffer, &profiler_,
                                       "Ready Frame");
      ReadyMeshDraw(command_buffer);

      ReadyCullData(command_buffer, render_scene_.forward_pass);
      ReadyCullData(command_buffer, render_scene_.transparent_pass);
      ReadyCullData(command_buffer, render_scene_.directional_shadow_pass);
      ReadyCullData(command_buffer, render_scene_.point_shadow_pass);

      vkCmdPipelineBarrier(command_buffer.Get(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           static_cast<uint32_t>(pre_cull_barriers_.size()),
                           pre_cull_barriers_.data(), 0, nullptr);
    }

    Renderer::CullParams forward_cull;
    forward_cull.view_mat = camera_.GetViewMat();
    forward_cull.proj_mat = camera_.GetProjMat(true);
    forward_cull.frustum_cull = *CVarSystem::Get()->GetIntCVar("culling.enable");
    forward_cull.occlusion_cull =
        *CVarSystem::Get()->GetIntCVar("culling.occlusion_culling");
    forward_cull.draw_dist =
        *CVarSystem::Get()->GetFloatCVar("culling.distance");

    Renderer::CullParams shadow_cull;
    shadow_cull.frustum_cull = false;
    shadow_cull.occlusion_cull = false;

    {
      Renderer::VulkanScopeTimer timer2(command_buffer, &profiler_,
                                       "Culling");

      ExecuteCull(command_buffer, render_scene_.forward_pass, forward_cull);
      ExecuteCull(command_buffer, render_scene_.transparent_pass, forward_cull);

      ExecuteCull(command_buffer, render_scene_.directional_shadow_pass,
                  shadow_cull);
      ExecuteCull(command_buffer, render_scene_.point_shadow_pass,
                  shadow_cull);
    
      vkCmdPipelineBarrier(command_buffer.Get(),
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr,
                           static_cast<uint32_t>(post_cull_barriers_.size()),
                           post_cull_barriers_.data(), 0, nullptr);
    }

    DrawShadows(command_buffer);

    DrawForward(command_buffer);

    if (forward_cull.occlusion_cull) ReduceDepth(command_buffer);

    CopyRenderToSwapchain(command_buffer, image_index);
  }

  VK_CHECK(command_buffer.End());

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &frame.present_semaphore;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &frame.render_semaphore;
  command_buffer.Submit(std::move(submit_info));

  VK_CHECK(device_.GetGraphicsQueue().SubmitBatches(frame.render_fence));

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  VkSwapchainKHR swapchains[] = {swapchain_.Get()};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &frame.render_semaphore;
  present_info.pImageIndices = &image_index;

  res = device_.GetPresentQueue().Present(&present_info);

  if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR ||
      window_.GetResized()) {
    window_.SetResized(false);
    RecreateSwapchain(frame.command_pool);
    VK_CHECK(vkResetFences(device_.Get(), 1, &window_resize_fence_));
    VK_CHECK(device_.GetGraphicsQueue().SubmitBatches(window_resize_fence_));
  } else if (res != VK_SUCCESS) {
    VK_CHECK(res);
  }

  frame_number_++;
}

void VulkanEngine::ReadyMeshDraw(Renderer::CommandBuffer command_buffer) {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  if (render_scene_.dirty_objects.size() > 0) {

    // Realloc if not enough space
    bool full_reupload = false;
    size_t copy_size =
        render_scene_.renderables.size() * sizeof(Renderer::GPUObjectData);
    if (copy_size > render_scene_.object_data_buffer.GetSize()) {
      frame.deletion_queue.PushFunction(std::bind(
          &Renderer::Buffer<false>::Destroy, render_scene_.object_data_buffer));
      render_scene_.object_data_buffer.Create(
          allocator_, copy_size,
          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
      full_reupload = true;
    }

    // Full reupload if too much changed
    constexpr float kFullReuploadCoefficient = 0.8f;
    full_reupload = full_reupload || render_scene_.dirty_objects.size() >=
                                         render_scene_.renderables.size() *
                                             kFullReuploadCoefficient;
    if (full_reupload) {
      Renderer::Buffer<true> staging_buffer(
          allocator_, copy_size,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
      Renderer::GPUObjectData* object_ssbo =
          staging_buffer.GetMappedMemory<Renderer::GPUObjectData>();
      render_scene_.FillObjectData(object_ssbo);

      frame.deletion_queue.PushFunction(
          std::bind(&Renderer::Buffer<true>::Destroy, staging_buffer));

      staging_buffer.CopyTo(command_buffer, render_scene_.object_data_buffer);
    } else {
      uint64_t buffer_size =
          sizeof(Renderer::GPUObjectData) * render_scene_.dirty_objects.size();
      uint64_t word_size = sizeof(Renderer::GPUObjectData) / sizeof(uint32_t);
      uint64_t upload_size =
          render_scene_.dirty_objects.size() * word_size * sizeof(uint32_t);

      Renderer::Buffer<true> new_buffer(
          allocator_, buffer_size,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
      Renderer::Buffer<true> target_buffer(
          allocator_, upload_size,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
      frame.deletion_queue.PushFunction(
          std::bind(&Renderer::Buffer<true>::Destroy, new_buffer));
      frame.deletion_queue.PushFunction(
          std::bind(&Renderer::Buffer<true>::Destroy, target_buffer));

      uint32_t* target_data = target_buffer.GetMappedMemory<uint32_t>();
      Renderer::GPUObjectData* object_data =
          new_buffer.GetMappedMemory<Renderer::GPUObjectData>();

      uint32_t sidx = 0;
      for (uint32_t i = 0; i < render_scene_.dirty_objects.size(); ++i) {
        render_scene_.WriteObject(object_data + i,
                                  render_scene_.dirty_objects[i]);
        uint32_t dst_offset = static_cast<uint32_t>(
            word_size * render_scene_.dirty_objects[i].handle);
        for (uint32_t j = 0; j < word_size; ++j) {
          target_data[sidx] = dst_offset + j;
          ++sidx;
        }
      }
      uint32_t launch_count = sidx;

      VkDescriptorBufferInfo index_data = target_buffer.GetDescriptorInfo();
      VkDescriptorBufferInfo source_data = new_buffer.GetDescriptorInfo();
      VkDescriptorBufferInfo target_info =
          render_scene_.object_data_buffer.GetDescriptorInfo();

      VkDescriptorSet upload_set;
      Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                         &frame.dynamic_descriptor_allocator)
          .BindBuffer(0, &index_data, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                      VK_SHADER_STAGE_COMPUTE_BIT)
          .BindBuffer(1, &source_data, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                      VK_SHADER_STAGE_COMPUTE_BIT)
          .BindBuffer(2, &target_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                      VK_SHADER_STAGE_COMPUTE_BIT)
          .Build(upload_set);

      vkCmdBindPipeline(command_buffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE,
                        sparse_upload_pipeline_);

      vkCmdPushConstants(command_buffer.Get(), sparse_upload_layout_,
                         VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t),
                         &launch_count);
      vkCmdBindDescriptorSets(
          command_buffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE,
          sparse_upload_layout_, 0, 1, &upload_set, 0, nullptr);

      vkCmdDispatch(command_buffer.Get(), launch_count / 256 + 1, 1, 1);
    }

    Renderer::BufferMemoryBarrier barrier(
        render_scene_.object_data_buffer,
        device_.GetQueueFamilies().graphics_family.value());
    barrier.SetSrcAccessMask(VK_ACCESS_TRANSFER_WRITE_BIT);
    barrier.SetDstAccessMask(VK_ACCESS_SHADER_WRITE_BIT |
                             VK_ACCESS_SHADER_READ_BIT);

    upload_barriers_.push_back(barrier.Get());

    render_scene_.ClearDirtyObjects();

    Renderer::RenderScene::MeshPass* passes[4] = {
        &render_scene_.forward_pass, &render_scene_.transparent_pass,
        &render_scene_.directional_shadow_pass,
        &render_scene_.point_shadow_pass};

    for (size_t i = 0; i < 4; ++i) {
      Renderer::RenderScene::MeshPass& pass = *passes[i];

      uint32_t count_size = static_cast<uint32_t>(
          pass.multibatches.size() * sizeof(uint32_t));
      if (pass.count_buffer.GetSize() < count_size) {
        frame.deletion_queue.PushFunction(std::bind(
            &Renderer::Buffer<false>::Destroy, pass.count_buffer));
        pass.count_buffer.Create(allocator_, count_size,
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
      }

      uint32_t draw_indirect_size = static_cast<uint32_t>(
          pass.indirect_batches.size() * sizeof(Renderer::GPUIndirectObject));
      if (pass.draw_indirect_buffer.GetSize() < draw_indirect_size) {
        frame.deletion_queue.PushFunction(std::bind(
            &Renderer::Buffer<false>::Destroy, pass.draw_indirect_buffer));
        pass.draw_indirect_buffer.Create(
            allocator_, draw_indirect_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
      }

      uint32_t compacted_instance_size =
          static_cast<uint32_t>(pass.batches.size() * sizeof(uint32_t));
      if (pass.compacted_instance_buffer.GetSize() < compacted_instance_size) {
        frame.deletion_queue.PushFunction(std::bind(
            &Renderer::Buffer<false>::Destroy, pass.compacted_instance_buffer));
        pass.compacted_instance_buffer.Create(
            allocator_, compacted_instance_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
      }

      uint32_t pass_objects_size = static_cast<uint32_t>(
          pass.batches.size() * sizeof(Renderer::GPUInstance));
      if (pass.pass_objects_buffer.GetSize() < pass_objects_size) {
        frame.deletion_queue.PushFunction(std::bind(
            &Renderer::Buffer<false>::Destroy, pass.pass_objects_buffer));
        pass.pass_objects_buffer.Create(
            allocator_, pass_objects_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
      }

      uint32_t multibatches_size = static_cast<uint32_t>(
          pass.multibatches.size() * sizeof(Renderer::RenderScene::Multibatch));
      if (pass.multibatches_buffer.GetSize() < multibatches_size) {
        frame.deletion_queue.PushFunction(std::bind(
            &Renderer::Buffer<false>::Destroy, pass.multibatches_buffer));
        pass.multibatches_buffer.Create(allocator_, multibatches_size,
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
      }
    }

    std::vector<std::future<void>> async_calls;
    async_calls.reserve(9);
    
    for (size_t i = 0; i < 4; ++i) {
      Renderer::RenderScene::MeshPass* pass = passes[i];
      Renderer::RenderScene* scene = &render_scene_;

      if (pass->needs_indirect_refresh && pass->indirect_batches.size() > 0) {
        if (pass->clear_indirect_buffer.Get() != VK_NULL_HANDLE) {
          frame.deletion_queue.PushFunction(std::bind(
              &Renderer::Buffer<true>::Destroy, pass->clear_indirect_buffer));
        }
        pass->clear_indirect_buffer.Create(
            allocator_,
            sizeof(Renderer::GPUIndirectObject) * pass->indirect_batches.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        Renderer::GPUIndirectObject* indirect =
            pass->clear_indirect_buffer
                .GetMappedMemory<Renderer::GPUIndirectObject>();
        async_calls.push_back(std::async(std::launch::async, [=]() {
          scene->FillIndirectArray(indirect, *pass);
        }));

        if (pass->clear_count_buffer.Get() != VK_NULL_HANDLE) {
          frame.deletion_queue.PushFunction(std::bind(
              &Renderer::Buffer<true>::Destroy, pass->clear_count_buffer));
        }
        pass->clear_count_buffer.Create(
            allocator_,
            sizeof(uint32_t) * pass->multibatches.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        async_calls.push_back(std::async(
            std::launch::async, [=]() { scene->ClearCountArray(*pass); }));

        if (pass->clear_multibatches_buffer.Get() != VK_NULL_HANDLE) {
          frame.deletion_queue.PushFunction(std::bind(&Renderer::Buffer<true>::Destroy,
                        pass->clear_multibatches_buffer));
        }
        pass->clear_multibatches_buffer.Create(
            allocator_,
            pass->multibatches.size() *
                sizeof(Renderer::RenderScene::Multibatch),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        Renderer::RenderScene::Multibatch* multibatch =
            pass->clear_multibatches_buffer
                .GetMappedMemory<Renderer::RenderScene::Multibatch>();
        async_calls.push_back(std::async(std::launch::async, [=]() {
          scene->FillMultibatchesArray(multibatch, *pass);
        }));

        pass->needs_indirect_refresh = false;
      }

      if (pass->needs_instance_refresh && pass->batches.size() > 0) {
        Renderer::Buffer<true> instance_staging;
        instance_staging.Create(
            allocator_, sizeof(Renderer::GPUInstance) * pass->batches.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
        frame.deletion_queue.PushFunction(
            std::bind(&Renderer::Buffer<true>::Destroy, instance_staging));

        Renderer::GPUInstance* instance =
            instance_staging.GetMappedMemory<Renderer::GPUInstance>();

        async_calls.push_back(std::async(std::launch::async, [=]() {
          scene->FillInstanceArray(instance, *pass);
        }));

        instance_staging.CopyTo(command_buffer, pass->pass_objects_buffer);

        upload_barriers_.push_back(barrier.Get());

        pass->needs_instance_refresh = false;
      }
    }

    for (auto& call : async_calls) call.get();

    vkCmdPipelineBarrier(command_buffer.Get(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                         static_cast<uint32_t>(upload_barriers_.size()),
                         upload_barriers_.data(), 0, nullptr);
    upload_barriers_.clear();
  }
}

void VulkanEngine::ReadyCullData(Renderer::CommandBuffer command_buffer,
                                 Renderer::RenderScene::MeshPass& pass) {
  if (pass.clear_indirect_buffer.Get() == VK_NULL_HANDLE) return;
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  pass.clear_indirect_buffer.CopyTo(command_buffer, pass.draw_indirect_buffer);

  pass.clear_count_buffer.CopyTo(command_buffer, pass.count_buffer);

  pass.clear_multibatches_buffer.CopyTo(command_buffer,
                                        pass.multibatches_buffer);

  Renderer::BufferMemoryBarrier barrier(
      pass.draw_indirect_buffer,
      device_.GetQueueFamilies().graphics_family.value());
  barrier.SetSrcAccessMask(VK_ACCESS_TRANSFER_WRITE_BIT);
  barrier.SetDstAccessMask(VK_ACCESS_SHADER_WRITE_BIT |
                           VK_ACCESS_SHADER_READ_BIT);
  pre_cull_barriers_.push_back(barrier.Get());

  barrier.SetBuffer(pass.count_buffer);
  pre_cull_barriers_.push_back(barrier.Get());

  barrier.SetBuffer(pass.multibatches_buffer);
  pre_cull_barriers_.push_back(barrier.Get());
}

void VulkanEngine::ExecuteCull(Renderer::CommandBuffer command_buffer,
                               const Renderer::RenderScene::MeshPass& pass,
                               const Renderer::CullParams& params) {
  if (pass.indirect_batches.size() == 0) return;
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  VkDescriptorBufferInfo object_buffer_info =
      render_scene_.object_data_buffer.GetDescriptorInfo();

  VkDescriptorBufferInfo indirect_info =
      pass.draw_indirect_buffer.GetDescriptorInfo();

  VkDescriptorBufferInfo instance_info =
      pass.pass_objects_buffer.GetDescriptorInfo();

  VkDescriptorBufferInfo multibach_info =
      pass.multibatches_buffer.GetDescriptorInfo();

  VkDescriptorBufferInfo final_info =
      pass.compacted_instance_buffer.GetDescriptorInfo();

  VkDescriptorBufferInfo count_info = pass.count_buffer.GetDescriptorInfo();

  VkDescriptorImageInfo depth_pyramid;
  depth_pyramid.sampler = depth_sampler_.Get();
  depth_pyramid.imageView = depth_pyramid_.GetView();
  depth_pyramid.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkDescriptorSet compute_set;
  Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                     &frame.dynamic_descriptor_allocator)
      .BindBuffer(0, &object_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                  VK_SHADER_STAGE_COMPUTE_BIT)
      .BindBuffer(1, &indirect_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                  VK_SHADER_STAGE_COMPUTE_BIT)
      .BindBuffer(2, &instance_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                  VK_SHADER_STAGE_COMPUTE_BIT)
      .BindBuffer(3, &final_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                  VK_SHADER_STAGE_COMPUTE_BIT)
      .BindImage(4, &depth_pyramid, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 VK_SHADER_STAGE_COMPUTE_BIT)
      .BindBuffer(5, &count_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                  VK_SHADER_STAGE_COMPUTE_BIT)
      .BindBuffer(6, &multibach_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                  VK_SHADER_STAGE_COMPUTE_BIT)
      .Build(compute_set);

  glm::mat4 projection = params.proj_mat;
  glm::mat4 projection_t = glm::transpose(projection);

  glm::vec4 frustum_x =
      (projection_t[3] + projection_t[0]) /
      glm::length(glm::vec3(projection_t[3] + projection_t[0]));
  glm::vec4 frustum_y =
      (projection_t[3] + projection_t[1]) /
      glm::length(glm::vec3(projection_t[3] + projection_t[1]));

  Renderer::DrawCullData cull_data{};
  cull_data.view = params.view_mat;
  cull_data.P00 = projection[0][0];
  cull_data.P11 = projection[1][1];
  cull_data.z_near = 0.1f;
  cull_data.z_far = params.draw_dist;
  cull_data.frustum[0] = frustum_x.x;
  cull_data.frustum[1] = frustum_x.z;
  cull_data.frustum[2] = frustum_y.y;
  cull_data.frustum[3] = frustum_y.z;
  cull_data.max_draw_count = static_cast<uint32_t>(pass.batches.size());
  cull_data.culling_enabled = params.frustum_cull;
  cull_data.occlusion_enabled = params.occlusion_cull;
  cull_data.pyramid_width = static_cast<float>(depth_pyramid_width_);
  cull_data.pyramid_height = static_cast<float>(depth_pyramid_height_);

  cull_data.dist_cull = (params.draw_dist > 10000.f ? 0 : 1);

  vkCmdBindPipeline(command_buffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE,
                    cull_pipeline_);

  vkCmdPushConstants(command_buffer.Get(), cull_layout_,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0,
                     sizeof(Renderer::DrawCullData), &cull_data);

  vkCmdBindDescriptorSets(command_buffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE,
                          cull_layout_, 0, 1, &compute_set, 0, nullptr);
  vkCmdDispatch(command_buffer.Get(),
                static_cast<uint32_t>(pass.batches.size() / 256 + 1), 1, 1);

  Renderer::BufferMemoryBarrier barrier(
      render_scene_.object_data_buffer,
      device_.GetQueueFamilies().graphics_family.value());
  barrier.SetSrcAccessMask(VK_ACCESS_SHADER_WRITE_BIT);
  barrier.SetDstAccessMask(VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
  post_cull_barriers_.push_back(barrier.Get());

  barrier.SetBuffer(pass.compacted_instance_buffer);
  post_cull_barriers_.push_back(barrier.Get());

  barrier.SetBuffer(pass.count_buffer);
  post_cull_barriers_.push_back(barrier.Get());
}

void VulkanEngine::DrawShadows(Renderer::CommandBuffer command_buffer) {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  Renderer::VulkanScopeTimer timer(command_buffer, &profiler_, "Shadow Draw");

  Renderer::GPUSceneData scene_data;
  scene_data.directional_lights_count =
      static_cast<uint32_t>(directional_lights_.size());
  for (size_t i = 0; i < directional_lights_.size(); ++i)
    scene_data.directional_lights[i] = directional_lights_.at(i).GetUniform();
  scene_data.point_lights_count = static_cast<uint32_t>(point_lights_.size());
  for (size_t i = 0; i < point_lights_.size(); ++i)
    scene_data.point_lights[i] = point_lights_[i].GetUniform();
  scene_data.spot_lights_count = static_cast<uint32_t>(spot_lights_.size());
  for (size_t i = 0; i < point_lights_.size(); ++i)
    scene_data.spot_lights[i] = spot_lights_[i].GetUniform();

  VkClearValue depth_clear;
  depth_clear.depthStencil.depth = 1.f;

  {
    Renderer::RenderScene::MeshPass& pass = render_scene_.directional_shadow_pass;
    VkDescriptorBufferInfo object_buffer_info =
        render_scene_.object_data_buffer.GetDescriptorInfo();
    VkDescriptorBufferInfo instance_info =
        pass.compacted_instance_buffer.GetDescriptorInfo();
    VkDescriptorSet object_set;
    Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                       &frame.dynamic_descriptor_allocator)
        .BindBuffer(0, &object_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_SHADER_STAGE_VERTEX_BIT)
        .BindBuffer(1, &instance_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_SHADER_STAGE_VERTEX_BIT)
        .Build(object_set);

    Renderer::RenderPass::BeginInfo begin_info{{depth_clear}};
    begin_info.framebuffer = shadow_framebuffer_.Get();
    begin_info.render_area = {{0, 0}, shadow_extent_};
    directional_shadow_pass_.Begin(command_buffer, begin_info);

    VkViewport viewport{0.f, 0.f,
                        static_cast<float>(shadow_extent_.width),
                        static_cast<float>(shadow_extent_.height),
                        0.f, 1.f};
    VkRect2D scissors{{0, 0}, shadow_extent_};

    vkCmdSetViewport(command_buffer.Get(), 0, 1, &viewport);
    vkCmdSetScissor(command_buffer.Get(), 0, 1, &scissors);

    uint32_t scene_offset = frame.dynamic_data.Push(scene_data);

    VkDescriptorBufferInfo scene_info = frame.dynamic_data.GetDescriptorInfo();
    scene_info.range = sizeof(Renderer::GPUSceneData);

    VkDescriptorSet global_set;
    Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                       &frame.dynamic_descriptor_allocator)
        .BindBuffer(0, &scene_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_GEOMETRY_BIT)
        .Build(global_set);

    vkCmdSetDepthBias(command_buffer.Get(), 0.f, 0.f, 1.2f);

    Renderer::DrawParams draw_params{};
    draw_params.offsets.push_back(scene_offset);
    draw_params.global_set = global_set;
    draw_params.object_data_set = object_set;

    ExecuteDraw(command_buffer, pass, draw_params);

    directional_shadow_pass_.End(command_buffer);
  }
  {
    Renderer::RenderScene::MeshPass& pass = render_scene_.point_shadow_pass;
    VkDescriptorBufferInfo object_buffer_info =
        render_scene_.object_data_buffer.GetDescriptorInfo();
    VkDescriptorBufferInfo instance_info =
        pass.compacted_instance_buffer.GetDescriptorInfo();
    VkDescriptorSet object_set;
    Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                       &frame.dynamic_descriptor_allocator)
        .BindBuffer(0, &object_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_SHADER_STAGE_VERTEX_BIT)
        .BindBuffer(1, &instance_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_SHADER_STAGE_VERTEX_BIT)
        .Build(object_set);

    VkExtent3D extent = point_shadow_image_.GetExtent();
    Renderer::RenderPass::BeginInfo begin_info{{depth_clear}};
    begin_info.framebuffer = point_shadow_framebuffer_.Get();
    begin_info.render_area = {{0, 0}, {extent.width, extent.height}};
    point_shadow_pass_.Begin(command_buffer, begin_info);

    VkViewport viewport{0.f, 0.f,
                        static_cast<float>(extent.width),
                        static_cast<float>(extent.height),
                        0.f, 1.f};
    VkRect2D scissors{{0, 0}, {extent.width, extent.height}};

    vkCmdSetViewport(command_buffer.Get(), 0, 1, &viewport);
    vkCmdSetScissor(command_buffer.Get(), 0, 1, &scissors);

    uint32_t scene_offset = frame.dynamic_data.Push(scene_data);

    VkDescriptorBufferInfo scene_info = frame.dynamic_data.GetDescriptorInfo();
    scene_info.range = sizeof(Renderer::GPUSceneData);

    VkDescriptorSet global_set;
    Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                       &frame.dynamic_descriptor_allocator)
        .BindBuffer(0, &scene_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build(global_set);

    vkCmdSetDepthBias(command_buffer.Get(), 0.f, 0.f, 1.2f);

    Renderer::DrawParams draw_params{};
    draw_params.offsets.push_back(scene_offset);
    draw_params.global_set = global_set;
    draw_params.object_data_set = object_set;

    ExecuteDraw(command_buffer, pass, draw_params);

    point_shadow_pass_.End(command_buffer);
  }

}

void VulkanEngine::DrawForward(Renderer::CommandBuffer command_buffer) {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];
  Renderer::RenderScene::MeshPass& pass = render_scene_.forward_pass;

  Renderer::VulkanScopeTimer timer(command_buffer, &profiler_, "Forward Draw");
  Renderer::VulkanPipelineStatRecorder stats(command_buffer, &profiler_,
                                             "Total Primitives");

  VkClearValue clear_value{};
  auto clear_color = CVarSystem::Get()->GetVec4CVar("scene.clear_color");
  clear_value.color = {
      {clear_color->x, clear_color->y, clear_color->z, clear_color->w}};
  VkClearValue depth_clear{};
  depth_clear.depthStencil.depth = 1.f;

  Renderer::RenderPass::BeginInfo begin_info{
      {clear_value, depth_clear, clear_value, depth_clear}};
  begin_info.framebuffer = forward_framebuffer_.Get();
  begin_info.render_area = {{0, 0}, swapchain_.GetImageExtent()};
  forward_pass_.Begin(command_buffer, begin_info);

  VkExtent2D extent = swapchain_.GetImageExtent();
  VkViewport viewport{0.f, 0.f,
                      static_cast<float>(extent.width),
                      static_cast<float>(extent.height),
                      0.f, 1.f};
  VkRect2D scissors{{0, 0}, extent};

  vkCmdSetViewport(command_buffer.Get(), 0, 1, &viewport);
  vkCmdSetScissor(command_buffer.Get(), 0, 1, &scissors);

  scene_data_.camera_data.view = camera_.GetViewMat();
  scene_data_.camera_data.projection = camera_.GetProjMat();
  scene_data_.camera_data.view_proj =
      scene_data_.camera_data.projection * scene_data_.camera_data.view;
  scene_data_.camera_data.pos = camera_.GetPosition();

  scene_data_.directional_lights_count =
      static_cast<uint32_t>(directional_lights_.size());
  for (size_t i = 0; i < directional_lights_.size(); ++i)
    scene_data_.directional_lights[i] = directional_lights_.at(i).GetUniform();

  scene_data_.spot_lights_count = static_cast<uint32_t>(spot_lights_.size());
  for (size_t i = 0; i < spot_lights_.size(); ++i)
    scene_data_.spot_lights[i] = spot_lights_.at(i).GetUniform();

  scene_data_.point_lights_count = static_cast<uint32_t>(point_lights_.size());
  for (size_t i = 0; i < point_lights_.size(); ++i)
    scene_data_.point_lights[i] = point_lights_[i].GetUniform();
  uint32_t scene_data_offset = frame.dynamic_data.Push(scene_data_);

  VkDescriptorBufferInfo object_buffer_info =
      render_scene_.object_data_buffer.GetDescriptorInfo();

  VkDescriptorBufferInfo scene_info = frame.dynamic_data.GetDescriptorInfo();
  scene_info.range = sizeof(Renderer::GPUSceneData);

  VkDescriptorBufferInfo instance_info =
      pass.compacted_instance_buffer.GetDescriptorInfo();

  VkDescriptorImageInfo shadow_info;
  shadow_info.sampler = shadow_sampler_.Get();
  shadow_info.imageView = shadow_image_.GetView();
  shadow_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkDescriptorImageInfo point_shadow_info;
  point_shadow_info.sampler = shadow_sampler_.Get();
  point_shadow_info.imageView = point_shadow_image_.GetView();
  point_shadow_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                     &frame.dynamic_descriptor_allocator)
      .BindBuffer(0, &scene_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
      .BindImage(1, &shadow_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
      .BindImage(2, &point_shadow_info, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
      .Build(frame.global_descriptor);

  Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                     &frame.dynamic_descriptor_allocator)
      .BindBuffer(0, &object_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                  VK_SHADER_STAGE_VERTEX_BIT)
      .BindBuffer(1, &instance_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                  VK_SHADER_STAGE_VERTEX_BIT)
      .Build(frame.object_descriptor);

  vkCmdSetDepthBias(command_buffer.Get(), 0.f, 0.f, 0.f);

  Renderer::DrawParams draw_params{};
  draw_params.offsets.push_back(scene_data_offset);
  draw_params.global_set = frame.global_descriptor;
  draw_params.object_data_set = frame.object_descriptor;

  ExecuteDraw(command_buffer, pass, draw_params);

  DrawSkybox(command_buffer, scene_info, scene_data_offset);

  DrawCoordAxes(command_buffer, scene_info, scene_data_offset);

  forward_pass_.End(command_buffer);
}

void VulkanEngine::ExecuteDraw(Renderer::CommandBuffer command_buffer,
                               const Renderer::RenderScene::MeshPass& pass,
                               const Renderer::DrawParams& draw_params) {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  if (pass.indirect_batches.size() == 0) return;

  Renderer::Mesh* last_mesh = nullptr;
  Renderer::Material* last_material = nullptr;
  VkPipeline last_pipeline = nullptr;
  VkDescriptorSet last_material_set = nullptr;

  VkDeviceSize offset = 0;
  VkBuffer vertex_buffer = render_scene_.merged_vertex_buffer.Get();
  vkCmdBindVertexBuffers(command_buffer.Get(), 0, 1, &vertex_buffer, &offset);
  vkCmdBindIndexBuffer(command_buffer.Get(),
                        render_scene_.merged_index_buffer.Get(), 0,
                        VK_INDEX_TYPE_UINT32);

  for (size_t i = 0; i < pass.multibatches.size(); ++i) {
    auto& multibatch = pass.multibatches[i];
    auto& instance = pass.indirect_batches[multibatch.first];

    Renderer::Pipeline new_pipeline = instance.material.shader_pass->pipeline;
    VkDescriptorSet new_material_set = instance.material.material_set;

    Renderer::Mesh* draw_mesh = render_scene_.GetMesh(instance.mesh_id)->mesh;

    if (new_pipeline.Get() != last_pipeline) {
      last_pipeline = new_pipeline.Get();
      new_pipeline.Bind(command_buffer);
      vkCmdBindDescriptorSets(
          command_buffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS,
          new_pipeline.GetLayout(), 0, 1, &draw_params.global_set,
          static_cast<uint32_t>(draw_params.offsets.size()),
          draw_params.offsets.data());
      vkCmdBindDescriptorSets(command_buffer.Get(),
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              new_pipeline.GetLayout(), 1, 1,
                              &draw_params.object_data_set, 0, nullptr);
    }

    if (new_material_set != last_material_set) {
      last_material_set = new_material_set;
      vkCmdBindDescriptorSets(
          command_buffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS,
          new_pipeline.GetLayout(), 2, 1, &new_material_set, 0, nullptr);
    }

    if (draw_params.push_constants.has_value()) {
      const Renderer::PushConstants& constants =
          draw_params.push_constants.value();
      vkCmdPushConstants(command_buffer.Get(), new_pipeline.GetLayout(),
                         constants.stages, constants.offset, constants.size,
                         constants.data);
    }

    bool merged = render_scene_.GetMesh(instance.mesh_id)->is_merged;
    if (merged) {
      if (last_mesh != nullptr) {
        VkDeviceSize offset = 0;
        VkBuffer vertex_buffer = render_scene_.merged_vertex_buffer.Get();
        vkCmdBindVertexBuffers(command_buffer.Get(), 0, 1, &vertex_buffer,
                                &offset);
        vkCmdBindIndexBuffer(command_buffer.Get(),
                              render_scene_.merged_index_buffer.Get(), 0,
                              VK_INDEX_TYPE_UINT32);
        last_mesh = nullptr;
      }
    } else if (last_mesh != draw_mesh) {
      VkDeviceSize offset = 0;
      VkBuffer vertex_buffer = draw_mesh->GetVertexBuffer().Get();
      vkCmdBindVertexBuffers(command_buffer.Get(), 0, 1, &vertex_buffer,
                              &offset);
      vkCmdBindIndexBuffer(command_buffer.Get(),
                            draw_mesh->GetIndexBuffer().Get(), 0,
                            VK_INDEX_TYPE_UINT32);
      last_mesh = draw_mesh;
    }

    bool has_indices = draw_mesh->GetIndicesCount() > 0;
    if (!has_indices) {
      vkCmdDraw(command_buffer.Get(), draw_mesh->GetVerticesCount(),
                instance.count, 0, instance.first);
    } else {
      vkCmdDrawIndexedIndirectCount(
          command_buffer.Get(), pass.draw_indirect_buffer.Get(),
          multibatch.first * sizeof(Renderer::GPUIndirectObject),
          pass.count_buffer.Get(), i * sizeof(uint32_t),
          multibatch.count, sizeof(Renderer::GPUIndirectObject));
    }

    bool show_normals = *CVarSystem::Get()->GetIntCVar("show_normals");
    if (show_normals && pass.type == Renderer::MeshPassType::kForward) {
      Renderer::Material* material =
          Renderer::MaterialSystem::GetMaterial("normals");
      Renderer::Pipeline pipeline =
          material->original->pass_shaders[Renderer::MeshPassType::kForward]
              ->pipeline;

      last_pipeline = pipeline.Get();

      VkDescriptorBufferInfo scene_info =
          frame.dynamic_data.GetDescriptorInfo();
      scene_info.range = sizeof(Renderer::GPUSceneData);

      VkDescriptorSet normals_global_set;
      Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                          &frame.dynamic_descriptor_allocator)
          .BindBuffer(
              0, &scene_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT)
          .Build(normals_global_set);

      pipeline.Bind(command_buffer);
      vkCmdBindDescriptorSets(
          command_buffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline.GetLayout(), 0, 1, &normals_global_set,
                              static_cast<uint32_t>(draw_params.offsets.size()),
                              draw_params.offsets.data());
      vkCmdBindDescriptorSets(
          command_buffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline.GetLayout(), 1, 1, &draw_params.object_data_set, 0, nullptr);

      if (!has_indices) {
        vkCmdDraw(command_buffer.Get(), draw_mesh->GetVerticesCount(),
                  instance.count, 0, instance.first);
      } else {
        vkCmdDrawIndexedIndirectCount(
            command_buffer.Get(), pass.draw_indirect_buffer.Get(),
            multibatch.first * sizeof(Renderer::GPUIndirectObject),
            pass.count_buffer.Get(), i * sizeof(uint32_t),
            multibatch.count, sizeof(Renderer::GPUIndirectObject));
      }
    }
  }
}

void VulkanEngine::DrawSkybox(Renderer::CommandBuffer command_buffer,
                              VkDescriptorBufferInfo scene_info,
                              uint32_t dynamic_offset) {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  Renderer::Material* material =
      Renderer::MaterialSystem::GetMaterial("skybox");
  Renderer::Pipeline pipeline =
      material->original->pass_shaders[Renderer::MeshPassType::kForward]
          ->pipeline;
  VkDescriptorSet skybox_set =
      material->pass_sets[Renderer::MeshPassType::kForward];

  VkDescriptorSet skybox_global_set;
  Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                     &frame.dynamic_descriptor_allocator)
      .BindBuffer(0, &scene_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT)
      .Build(skybox_global_set);

  pipeline.Bind(command_buffer);
  vkCmdBindDescriptorSets(command_buffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline.GetLayout(), 0, 1, &skybox_global_set, 1,
                          &dynamic_offset);
  vkCmdBindDescriptorSets(command_buffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline.GetLayout(), 1, 1, &skybox_set, 0, nullptr);

  Renderer::Mesh* cube = GetMesh("cube");
  cube->BindBuffers(command_buffer);
  vkCmdDrawIndexed(command_buffer.Get(), cube->GetIndicesCount(), 1, 0, 0, 0);
}

void VulkanEngine::DrawCoordAxes(Renderer::CommandBuffer command_buffer,
                                 VkDescriptorBufferInfo scene_info,
                                 uint32_t dynamic_offset) {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  VkDescriptorSet axes_global_set;
  Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                     &frame.dynamic_descriptor_allocator)
      .BindBuffer(0, &scene_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_GEOMETRY_BIT)
      .Build(axes_global_set);

  axes_pipeline_.Bind(command_buffer);
  vkCmdBindDescriptorSets(command_buffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          axes_pipeline_.GetLayout(), 0, 1, &axes_global_set, 1,
                          &dynamic_offset);

  VkBuffer vertex_buffers[] = {axes_buffer_.Get()};
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command_buffer.Get(), 0, 1, vertex_buffers, &offset);

  vkCmdDraw(command_buffer.Get(), 1, 1, 0, 0);
}

struct alignas(16) DepthReduceData {
  glm::vec2 image_size;
};

inline uint32_t GetGroupCount(uint32_t thread_count, uint32_t local_size) {
  return (thread_count + local_size - 1) / local_size;
}

void VulkanEngine::ReduceDepth(Renderer::CommandBuffer command_buffer) {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  Renderer::VulkanScopeTimer timer(command_buffer, &profiler_, "Depth Reduce");

  Renderer::Image::LayoutTransitionInfo layout_info{};
  layout_info.src_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  layout_info.dst_access = VK_ACCESS_SHADER_READ_BIT;
  layout_info.new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  layout_info.src_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  layout_info.dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  layout_info.aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
  layout_info.dependency = VK_DEPENDENCY_BY_REGION_BIT;
  depth_resolve_image_.LayoutTransition(command_buffer, layout_info);

  vkCmdBindPipeline(command_buffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE,
                    depth_reduce_pipeline_);

  for (int32_t i = 0; i < depth_pyramid_levels_; ++i) {
    VkDescriptorImageInfo dst;
    dst.sampler = depth_reduction_sampler_.Get();
    dst.imageView = depth_pyramid_mips_[i];
    dst.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo src;
    src.sampler = depth_reduction_sampler_.Get();
    if (i == 0) {
      src.imageView = depth_resolve_image_.GetView();
      src.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    } else {
      src.imageView = depth_pyramid_mips_[i - 1];
      src.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    VkDescriptorSet depth_set;
    Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                       &frame.dynamic_descriptor_allocator)
        .BindImage(0, &dst, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                   VK_SHADER_STAGE_COMPUTE_BIT)
        .BindImage(1, &src, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_COMPUTE_BIT)
        .Build(depth_set);

    vkCmdBindDescriptorSets(command_buffer.Get(),
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            depth_reduce_layout_, 0, 1, &depth_set, 0, nullptr);

    uint32_t level_width = depth_pyramid_width_ >> i;
    uint32_t level_height = depth_pyramid_height_ >> i;
    if (level_width < 1) level_width = 1;
    if (level_height < 1) level_height = 1;

    DepthReduceData reduce_data = {glm::vec2(level_width, level_height)};

    vkCmdPushConstants(command_buffer.Get(), depth_reduce_layout_,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(reduce_data),
                       &reduce_data);
    vkCmdDispatch(command_buffer.Get(), GetGroupCount(level_width, 32),
                  GetGroupCount(level_height, 32), 1);

    Renderer::Image::LayoutTransitionInfo mip_layout_info{};
    mip_layout_info.src_access = VK_ACCESS_SHADER_WRITE_BIT;
    mip_layout_info.dst_access = VK_ACCESS_SHADER_READ_BIT;
    mip_layout_info.new_layout = VK_IMAGE_LAYOUT_GENERAL;
    mip_layout_info.src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    mip_layout_info.dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    mip_layout_info.aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    mip_layout_info.dependency = VK_DEPENDENCY_BY_REGION_BIT;
    depth_pyramid_.LayoutTransition(command_buffer, mip_layout_info);
  }

  layout_info.src_access = VK_ACCESS_SHADER_READ_BIT;
  layout_info.dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  layout_info.new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  layout_info.src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  layout_info.dst_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  layout_info.aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
  layout_info.dependency = VK_DEPENDENCY_BY_REGION_BIT;
  depth_resolve_image_.LayoutTransition(
      command_buffer, layout_info);
}

void VulkanEngine::CopyRenderToSwapchain(Renderer::CommandBuffer command_buffer,
                                         uint32_t index) {
  const uint32_t frame_index = frame_number_ % kMaxFramesInFlight;
  FrameData& frame = frames_[frame_index];

  Renderer::VulkanScopeTimer timer(command_buffer, &profiler_, "Copy to Swapchain");

  VkClearValue clear_value = {{{0.f, 0.f, 0.f, 1.f}}};
  Renderer::RenderPass::BeginInfo begin_info{{clear_value}};
  begin_info.framebuffer = swapchain_framebuffers_[index].Get();
  begin_info.render_area = {{0, 0}, swapchain_.GetImageExtent()};
  copy_pass_.Begin(command_buffer, begin_info);

  VkExtent2D extent = swapchain_.GetImageExtent();
  VkViewport viewport{0.f, 0.f,
                      static_cast<float>(extent.width),
                      static_cast<float>(extent.height),
                      0.f, 1.f};
  VkRect2D scissors{{0, 0}, extent};

  vkCmdSetViewport(command_buffer.Get(), 0, 1, &viewport);
  vkCmdSetScissor(command_buffer.Get(), 0, 1, &scissors);

  blit_pipeline_.Bind(command_buffer);

  VkDescriptorImageInfo source_image;
  source_image.sampler = smooth_sampler_.Get();
  source_image.imageView = color_resolve_image_.GetView();
  source_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkDescriptorSet blit_set;
  Renderer::DescriptorBuilder::Begin(&layout_cache_,
                                     &frame.dynamic_descriptor_allocator)
      .BindImage(0, &source_image, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
      .Build(blit_set);

  vkCmdBindDescriptorSets(command_buffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          blit_pipeline_.GetLayout(), 0, 1, &blit_set, 0,
                          nullptr);

  float exposure = *CVarSystem::Get()->GetFloatCVar("hdr_exposure");
  vkCmdPushConstants(command_buffer.Get(), blit_pipeline_.GetLayout(),
                     VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &exposure);

  vkCmdDraw(command_buffer.Get(), 3, 1, 0, 0);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer.Get());

  copy_pass_.End(command_buffer);
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

    directional_lights_[0].SetDirection(
        *CVarSystem::Get()->GetVec3CVar("scene.sunlight_dir"));
    directional_lights_[0].SetColor(
        *CVarSystem::Get()->GetVec4CVar("scene.sunlight_color"));

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