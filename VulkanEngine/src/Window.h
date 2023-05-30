#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

class VulkanEngine;

namespace Renderer {

class Window {
 public:
  Window();
  ~Window();

  void Init(uint32_t width, uint32_t height, const char* title,
            VulkanEngine* engine);
  void Destroy();

  GLFWwindow* GetWindow();

  bool ShouldClose() const;

  void PollEvents() const;
  void WaitEvents() const;

  bool GetResized() const;
  void SetResized(bool resized);

  void GetFramebufferSize(int* width, int* height) const;
  VkExtent2D GetFramebufferSize() const;

  static void FramebufferResizeCallback(GLFWwindow* window, int width,
                                        int height);

  static void CursorPosCallback(GLFWwindow* window, double x, double y);

 private:
  bool resized_;
  VulkanEngine* engine_;

  GLFWwindow* window_;
};

}  // namespace Renderer