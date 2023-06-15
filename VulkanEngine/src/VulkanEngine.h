#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Camera.h"
#include "CommandPool.h"
#include "DeletionQueue.h"
#include "Descriptors.h"
#include "Image.h"
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
#include "SwapchainFramebuffers.h"
#include "Texture.h"
#include "TextureSampler.h"
#include "VulkanInstance.h"
#include "VulkanProfiler.h"
#include "Window.h"

#include "PrefabAsset.h"

#define VMA_VULKAN_VERSION 1002000
#include <vma\include\vk_mem_alloc.h>

#include <glm\glm.hpp>

namespace Renderer {
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
}

namespace Engine {

constexpr uint32_t kMaxFramesInFlight = 2;

struct FrameData {
  Renderer::CommandPool command_pool;

  VkSemaphore render_semaphore, present_semaphore;
  VkFence render_fence;

  Renderer::PushBuffer dynamic_data;
  Renderer::Buffer<true> object_buffer;

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

  void Draw();
  void ReadyMeshDraw(Renderer::CommandBuffer command_buffer);
  void DrawForward(Renderer::CommandBuffer command_buffer,
                   Renderer::RenderScene::MeshPass& pass);

  void DrawMenu();
  void DrawToolbar();
  void Run();

  void MousePosCallback(double x, double y);
  void KeyCallback(int key, int action, int mods);

  VmaAllocator GetAllocator();

 private:
  void InitCVars();
  void InitRenderPasses();
  void InitSyncStructures();
  void InitDescriptors();
  bool LoadMesh(Renderer::CommandBuffer command_buffer, const char* name,
                const char* path);
  bool LoadTexture(Renderer::CommandBuffer command_buffer, const char* name,
                   const char* path);
  bool LoadPrefab(Renderer::CommandBuffer command_buffer, const char* path,
                  glm::mat4 root = glm::mat4{1.f});
  void InitScene(Renderer::CommandPool& init_pool);
  void InitImgui(Renderer::CommandPool& init_pool);

  void RecreateSwapchain();

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
  Renderer::Camera camera_;

  Renderer::VulkanInstance instance_;
  Renderer::Surface surface_;
  Renderer::PhysicalDevice physical_device_;
  Renderer::LogicalDevice device_;

  Renderer::VulkanProfiler profiler_;

  VmaAllocator allocator_;
  DeletionQueue main_deletion_queue_;

  Renderer::Swapchain swapchain_;
  Renderer::Image color_image_;
  Renderer::Image depth_image_;
  Renderer::RenderPass render_pass_;
  Renderer::SwapchainFramebuffers swapchain_framebuffers_;

  Renderer::DescriptorAllocator descriptor_allocator_;
  Renderer::DescriptorLayoutCache layout_cache_;
  VkDescriptorPool imgui_pool_;

  std::array<FrameData, kMaxFramesInFlight> frames_;
  Renderer::SceneData scene_data_;
  Renderer::CommandPool upload_pool_;

  Renderer::ShaderCache shader_cache_;

  Renderer::RenderScene render_scene_;

  std::vector<Renderer::RenderObject> renderables_;

  std::unordered_map<std::string, Renderer::Material> materials_;
  std::unordered_map<std::string, Renderer::Mesh> meshes_;
  std::unordered_map<std::string, Renderer::Texture> textures_;
  std::unordered_map<std::string, Assets::PrefabInfo*> prefab_cache_;

  std::deque<std::string> prefabs_to_load_;

  Renderer::TextureSampler texture_sampler_;
};

}  // namespace Engine