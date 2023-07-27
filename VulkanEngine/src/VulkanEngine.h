#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Camera.h"
#include "CommandPool.h"
#include "DeletionQueue.h"
#include "Descriptors.h"
#include "Framebuffer.h"
#include "Image.h"
#include "Light.h"
#include "LimitedVector.h"
#include "LogicalDevice.h"
#include "MaterialSystem.h"
#include "Mesh.h"
#include "PhysicalDevice.h"
#include "Pipeline.h"
#include "PushBuffer.h"
#include "RenderObject.h"
#include "RenderPass.h"
#include "Scene.h"
#include "Shaders.h"
#include "Surface.h"
#include "Swapchain.h"
#include "Texture.h"
#include "TextureCube.h"
#include "TextureSampler.h"
#include "VulkanInstance.h"
#include "VulkanProfiler.h"
#include "Window.h"

#include "PrefabAsset.h"

#define VMA_VULKAN_VERSION 1002000
#include <vma\include\vk_mem_alloc.h>

namespace Renderer {

struct GPUCameraData {
  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 view_proj;
  alignas(16) glm::vec3 pos;
};

constexpr uint32_t kMaxDirectionalLights = 1;
constexpr uint32_t kMaxPointLights = 2;
constexpr uint32_t kMaxSpotLights = 2;

struct GPUSceneData {
  GPUCameraData camera_data;
  glm::vec4 fog_color;
  glm::vec4 fog_distances;
  uint32_t directional_lights_count;
  GPUDirectionalLight directional_lights[kMaxDirectionalLights];
  uint32_t point_lights_count;
  GPUPointLight point_lights[kMaxPointLights];
  uint32_t spot_lights_count;
  GPUSpotLight spot_lights[kMaxSpotLights];
};

struct GPUObjectData {
  glm::mat4 model_matrix;
  glm::mat4 normal_matrix;
  glm::vec4 origin_radius;
  glm::vec4 extents;
};

struct CullParams {
  glm::mat4 view_mat;
  glm::mat4 proj_mat;
  bool occlusion_cull;
  bool frustum_cull;
  float draw_dist;
};

struct DrawCullData {
  glm::mat4 view;
  float P00, P11, z_near, z_far;
  float frustum[4];
  float pyramid_width, pyramid_height;

  uint32_t max_draw_count;

  int culling_enabled;
  int occlusion_enabled;
  int dist_cull;
};

struct PushConstants {
  VkShaderStageFlags stages;
  uint32_t offset;
  uint32_t size;
  const void* data;
};

struct DrawParams {
  VkDescriptorSet object_data_set;
  std::vector<uint32_t> offsets;
  VkDescriptorSet global_set;
  std::optional<PushConstants> push_constants;
};

}

namespace Engine {

constexpr uint32_t kMaxFramesInFlight = 2;

struct FrameData {
  Renderer::CommandPool command_pool;
  Renderer::DescriptorAllocator dynamic_descriptor_allocator;

  VkSemaphore render_semaphore, present_semaphore;
  VkFence render_fence;

  Renderer::PushBuffer dynamic_data;

  VkDescriptorSet global_descriptor;
  VkDescriptorSet object_descriptor;

  DeletionQueue deletion_queue;
};

class VulkanEngine {
  friend class Renderer::MaterialSystem;
  friend class Renderer::RenderScene;
 public:
  VulkanEngine();
  ~VulkanEngine();

  void Init();
  void Cleanup();

  void MousePosCallback(double x, double y);
  void KeyCallback(int key, int action, int mods);

  VmaAllocator GetAllocator();
  
  void Run();

 private:
  void Draw();
  void ReadyMeshDraw(Renderer::CommandBuffer command_buffer);
  void ReadyCullData(Renderer::CommandBuffer command_buffer,
                        Renderer::RenderScene::MeshPass& pass);
  void ExecuteCull(Renderer::CommandBuffer command_buffer,
                   const Renderer::RenderScene::MeshPass& pass,
                   const Renderer::CullParams& params);
  void DrawShadows(Renderer::CommandBuffer command_buffer);
  void DrawForward(Renderer::CommandBuffer command_buffer);
  void ExecuteDraw(Renderer::CommandBuffer command_buffer,
                   const Renderer::RenderScene::MeshPass& pass,
                   const Renderer::DrawParams& draw_params);
  void DrawSkybox(Renderer::CommandBuffer command_buffer,
                  VkDescriptorBufferInfo scene_info, uint32_t dynamic_offset);
  void DrawCoordAxes(Renderer::CommandBuffer command_buffer,
                     VkDescriptorBufferInfo scene_info,
                     uint32_t dynamic_offset);
  void ReduceDepth(Renderer::CommandBuffer command_buffer);
  void PostProcessing(Renderer::CommandBuffer command_buffer);
  void CopyRenderToSwapchain(Renderer::CommandBuffer command_buffer,
                             uint32_t index); 

  void DrawMenu();
  void DrawToolbar();

  void InitCVars();
  void InitRenderPasses(VkSampleCountFlagBits samples);
  void InitFramebuffers();
  void InitSyncStructures();
  void InitDescriptors();
  void InitPipelines();
  void InitSamplers();
  void InitDepthPyramid(Renderer::CommandPool& init_pool);
  bool LoadComputeShader(const char* path, VkPipeline& pipeline,
                         VkPipelineLayout& layout);
  bool LoadMesh(Renderer::CommandBuffer command_buffer, const char* name,
                const char* path);
  bool LoadTexture(Renderer::CommandBuffer command_buffer, const char* name,
                   const char* path);
  bool LoadPrefab(Renderer::CommandBuffer command_buffer, const char* path,
                  glm::mat4 root = glm::mat4{1.f});
  void InitScene(Renderer::CommandPool& init_pool);
  void InitImgui(Renderer::CommandPool& init_pool);

  void RecreateSwapchain(Renderer::CommandPool& command_pool);

  std::string AssetPath(std::string_view path);
  Renderer::Mesh* GetMesh(const std::string& name);

  void EnableCursor(bool enable);
  void ProcessInput();

  bool is_initialized_;
  uint32_t frame_number_;
  float delta_time_;
  float last_time_;
  int last_mouse_x_, last_mouse_y_;
  bool cursor_enabled_;
  bool menu_opened_;

  Renderer::Window window_;

  Renderer::VulkanInstance instance_;
  Renderer::Surface surface_;
  Renderer::PhysicalDevice physical_device_;
  Renderer::LogicalDevice device_;

  Renderer::VulkanProfiler profiler_;

  VmaAllocator allocator_;
  DeletionQueue main_deletion_queue_;

  VkFormat color_attachment_format_;
  VkFormat normalized_color_format_;
  VkFormat depth_attachment_format_;
  VkSampleCountFlagBits samples_;
  Renderer::Swapchain swapchain_;
  Renderer::Image color_image_;
  Renderer::Image bright_image_;
  Renderer::Image depth_image_;
  Renderer::Image color_resolve_image_;
  Renderer::Image bright_resolve_image_;
  Renderer::Image depth_resolve_image_;
  Renderer::Image shadow_image_;
  VkExtent2D shadow_extent_{2048, 2048};
  Renderer::ImageCube point_shadow_image_;
  std::array<Renderer::Image, 2> blur_images_;

  Renderer::Image depth_pyramid_;
  uint32_t depth_pyramid_width_;
  uint32_t depth_pyramid_height_;
  uint32_t depth_pyramid_levels_;
  VkImageView depth_pyramid_mips_[16] = {};

  Renderer::RenderPass forward_pass_;
  Renderer::RenderPass directional_shadow_pass_;
  Renderer::RenderPass point_shadow_pass_;
  Renderer::RenderPass copy_pass_;
  Renderer::Framebuffer forward_framebuffer_;
  Renderer::Framebuffer shadow_framebuffer_;
  Renderer::Framebuffer point_shadow_framebuffer_;
  std::array<Renderer::Framebuffer, kMaxFramesInFlight> swapchain_framebuffers_;

  Renderer::DescriptorAllocator descriptor_allocator_;
  Renderer::DescriptorLayoutCache layout_cache_;
  VkDescriptorPool imgui_pool_;

  std::array<FrameData, kMaxFramesInFlight> frames_;
  VkFence window_resize_fence_;
  Renderer::GPUSceneData scene_data_;
  Renderer::CommandPool upload_pool_;

  std::vector<VkBufferMemoryBarrier> upload_barriers_;
  std::vector<VkBufferMemoryBarrier> pre_cull_barriers_;
  std::vector<VkBufferMemoryBarrier> post_cull_barriers_;

  VkPipeline sparse_upload_pipeline_;
  VkPipelineLayout sparse_upload_layout_;

  VkPipeline cull_pipeline_;
  VkPipelineLayout cull_layout_;

  VkPipeline depth_reduce_pipeline_;
  VkPipelineLayout depth_reduce_layout_;

  VkPipeline blur_pipeline_;
  VkPipelineLayout blur_pipeline_layout_;

  Renderer::Pipeline blit_pipeline_;

  Renderer::Pipeline skybox_pipeline_;

  Renderer::Pipeline axes_pipeline_;
  Renderer::VertexBuffer axes_buffer_;

  Renderer::ShaderCache shader_cache_;

  Renderer::RenderScene render_scene_;
  Renderer::Camera camera_;
  limited_vector<Renderer::DirectionalLight, Renderer::kMaxDirectionalLights>
      directional_lights_;
  limited_vector<Renderer::SpotLight, Renderer::kMaxSpotLights> spot_lights_;
  limited_vector<Renderer::PointLight, Renderer::kMaxPointLights> point_lights_;
  Renderer::TextureCube skybox_texture_;

  std::unordered_map<std::string, Renderer::Material> materials_;
  std::unordered_map<std::string, Renderer::Mesh> meshes_;
  std::unordered_map<std::string, Renderer::Texture> textures_;
  std::unordered_map<std::string, Assets::PrefabInfo*> prefab_cache_;

  std::deque<std::string> prefabs_to_load_;

  Renderer::TextureSampler texture_sampler_;
  Renderer::TextureSampler depth_sampler_;
  Renderer::TextureSampler depth_reduction_sampler_;
  Renderer::TextureSampler smooth_sampler_;
  Renderer::TextureSampler skybox_sampler_;
  Renderer::TextureSampler shadow_sampler_;
};

}  // namespace Engine