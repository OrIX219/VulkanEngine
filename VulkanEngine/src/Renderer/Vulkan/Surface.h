#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include "VulkanInstance.h"
#include "Window.h"

namespace Renderer {

class Surface {
 public:
  Surface();
  ~Surface();

  VkResult Init(VulkanInstance* instance, Window* window);
  void Destroy();

  VkSurfaceKHR Get();

  Window* GetWindow();

 private:
  VkSurfaceKHR surface_;

  VulkanInstance* instance_;
  Window* window_;
};

}  // namespace Renderer