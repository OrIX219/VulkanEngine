#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

namespace Renderer {

class Window {
 public:
  Window();
  ~Window();

  void Init(uint32_t width, uint32_t height, const char* title);
  void Destroy();

  GLFWwindow* GetWindow();

  bool ShouldClose() const;

  void PollEvents() const;
  void WaitEvents() const;

  bool GetResized() const;
  void SetResized(bool resized);

  void GetFramebufferSize(int* width, int* height) const;

 private:
  static void FramebufferResizeCallback(GLFWwindow* window, int width,
                                        int height) {
    auto wnd = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    wnd->resized_ = true;
  }

  bool resized_;

  GLFWwindow* window_;
};

}  // namespace Renderer