#pragma once

#include <string>
#include <vector>
#include <unordered_map>

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

  void Run();

 private:
  void InitSyncStructures();
  void InitDescriptors();
  void InitPipelines();
  void LoadMeshes();
  void LoadTextures();
  void InitScene();
  void InitImgui();

  Renderer::Material* CreateMaterial(Renderer::Pipeline pipeline,
                                     const std::string& name);
  Renderer::Material* GetMaterial(const std::string& name);
  Renderer::Mesh* GetMesh(const std::string& name); 
  size_t PadUniformBuffer(size_t size);

  bool is_initialized_;
  uint32_t frame_number_;

  Renderer::Window window_;

  Renderer::VulkanInstance instance_;
  Renderer::Surface surface_;
  Renderer::PhysicalDevice physical_device_;
  Renderer::LogicalDevice device_;

  VmaAllocator allocator_;
 
  Renderer::Swapchain swapchain_;
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