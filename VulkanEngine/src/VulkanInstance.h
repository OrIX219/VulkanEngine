#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

namespace Renderer {

class VulkanInstance {
 public:
  VulkanInstance();
  ~VulkanInstance();

  VkResult Init(bool enable_validation_layers,
                std::vector<const char*> validation_layers = {});

  void Destroy();

  VkInstance GetInstance();

  bool ValidationLayersEnabled() const;

  const std::vector<const char*>& GetValidationLayers() const;

 private:
  bool CheckValidationLayerSupport();

  std::vector<const char*> GetRequiredExtensions();

  VkInstance instance_;

  bool enable_validation_layers_;
  std::vector<const char*> validation_layers_;
};

}  // namespace Renderer