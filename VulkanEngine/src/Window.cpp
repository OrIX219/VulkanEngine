#include "Window.h"

namespace Renderer {

Window::Window() {}

Window::~Window() {}

void Window::Init(uint32_t width, uint32_t height, const char* title) {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
  glfwSetWindowUserPointer(window_, this);
  glfwSetWindowSizeCallback(window_, FramebufferResizeCallback);
}

void Window::Destroy() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

GLFWwindow* Window::GetWindow() { return window_; }

bool Window::ShouldClose() const { return glfwWindowShouldClose(window_); }

void Window::PollEvents() const { glfwPollEvents(); }

void Window::WaitEvents() const { glfwWaitEvents(); }

bool Window::GetResized() const { return resized_; }

void Window::SetResized(bool resized) { resized_ = resized; }

void Window::GetFramebufferSize(int* width, int* height) const {
  glfwGetFramebufferSize(window_, width, height);
}

}  // namespace Renderer