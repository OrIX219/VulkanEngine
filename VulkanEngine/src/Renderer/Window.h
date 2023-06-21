#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Engine {
class VulkanEngine;
}

namespace Renderer {

class Window {
 public:
  Window();
  ~Window();

  void Init(uint32_t width, uint32_t height, const char* title,
            Engine::VulkanEngine* engine);
  void Destroy();

  GLFWwindow* GetWindow();

  bool ShouldClose() const;
  void Close();

  void PollEvents() const;
  void WaitEvents() const;

  bool GetResized() const;
  void SetResized(bool resized);

  void GetFramebufferSize(int* width, int* height) const;
  VkExtent2D GetFramebufferSize() const;

  static void FramebufferResizeCallback(GLFWwindow* window, int width,
                                        int height);

  static void CursorPosCallback(GLFWwindow* window, double x, double y);

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action,
                          int mods);

 private:
  bool resized_;
  Engine::VulkanEngine* engine_;

  GLFWwindow* window_;
};

}  // namespace Renderer