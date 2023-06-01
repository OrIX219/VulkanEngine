#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Camera.h"
#include "CommandPool.h"
#include "Descriptors.h"
#include "DescriptorPool.h"
#include "Image.h"
#include "LogicalDevice.h"
#include "Mesh.h"
#include "PhysicalDevice.h"
#include "Pipeline.h"
#include "RenderObject.h"
#include "RenderPass.h"
#include "Surface.h"
#include "Swapchain.h"
#include "SwapchainFramebuffers.h"
#include "Texture.h"
#include "TextureSampler.h"
#include "VulkanInstance.h"
#include "Window.h"

#define VMA_VULKAN_VERSION 1002000
#include <vma\vk_mem_alloc.h>

#include <glm\glm.hpp>

constexpr uint32_t kMaxFramesInFlight = 2;

struct CameraData {
  glm::mat4 view;
  glm::mat4 projection;
};

struct SceneData {
  CameraData camera_data;
  glm::vec4 ambient_color;
  glm::vec4 fog_color;
  glm::vec4 fog_distances;
  glm::vec4 sunlight_direction;
  glm::vec4 sunlight_color;
};

struct ObjectData {
  glm::mat4 model_matrix;
};

struct FrameData {
  Renderer::CommandPool command_pool_;

  VkSemaphore render_semaphore_, present_semaphore_;
  VkFence render_fence_;

  Renderer::Buffer<true> object_buffer_;

  VkDescriptorSet global_descriptor_;
  VkDescriptorSet object_descriptor_;
};

class VulkanEngine {
 public:
  VulkanEngine();
  ~VulkanEngine();

  void Init();

  void Cleanup();

  void Draw();
  void DrawObjects(Renderer::CommandBuffer command_buffer,
                   Renderer::RenderObject* first, size_t count);

  void DrawMenu();
  void DrawToolbar();
  void Run();

  void MousePosCallback(double x, double y);
  void KeyCallback(int key, int action, int mods);

 private:
  void InitCVars();
  void InitRenderPasses();
  void InitSyncStructures();
  void InitDescriptors();
  void InitPipelines();
  void LoadMeshes();
  void LoadTextures();
  void InitScene();
  void InitImgui();

  void RecreateSwapchain();

  Renderer::Material* CreateMaterial(Renderer::Pipeline pipeline,
                                     const std::string& name);
  Renderer::Material* GetMaterial(const std::string& name);
  Renderer::Mesh* GetMesh(const std::string& name); 
  size_t PadUniformBuffer(size_t size);

  void EnableCursor(bool enable);
  void ProcessInput();

  bool is_initialized_;
  uint32_t frame_number_;
  float delta_time_;
  float last_time_;
  int last_mouse_x_, last_mouse_y_;
  bool cursor_enabled_;
  bool menu_opened_, console_opened_;

  Renderer::Window window_;
  Renderer::Camera camera_;

  Renderer::VulkanInstance instance_;
  Renderer::Surface surface_;
  Renderer::PhysicalDevice physical_device_;
  Renderer::LogicalDevice device_;

  VmaAllocator allocator_;
  Renderer::CommandPool init_pool_;

  Renderer::Swapchain swapchain_;
  Renderer::Image color_image_;
  Renderer::Image depth_image_;
  Renderer::RenderPass render_pass_;
  Renderer::SwapchainFramebuffers swapchain_framebuffers_;

  Renderer::DescriptorAllocator descriptor_allocator_;
  Renderer::DescriptorLayoutCache layout_cache_;
  Renderer::DescriptorPool imgui_pool_;

  VkDescriptorSetLayout global_set_layout_;
  VkDescriptorSetLayout object_set_layout_;
  VkDescriptorSetLayout single_texture_set_layout_;

  std::array<FrameData, kMaxFramesInFlight> frames_;
  SceneData scene_data_;
  Renderer::Buffer<true> scene_buffer_;

  std::vector<Renderer::RenderObject> renderables_;

  std::unordered_map<std::string, Renderer::Material> materials_;
  std::unordered_map<std::string, Renderer::Mesh> meshes_;
  std::unordered_map<std::string, Renderer::Texture> textures_;
  Renderer::TextureSampler texture_sampler_;
};