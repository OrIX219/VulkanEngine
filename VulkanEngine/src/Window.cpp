#include "Window.h"

#include "VulkanEngine.h"

namespace Renderer {

Window::Window() : resized_(false) {}

Window::~Window() {}

void Window::Init(uint32_t width, uint32_t height, const char* title,
                  VulkanEngine* engine) {
  engine_ = engine;

  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
  glfwSetWindowUserPointer(window_, this);
  glfwSetWindowSizeCallback(window_, FramebufferResizeCallback);
  glfwSetCursorPosCallback(window_, CursorPosCallback);
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

VkExtent2D Window::GetFramebufferSize() const {
  int width, height;
  glfwGetFramebufferSize(window_, &width, &height);
  return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

void Window::FramebufferResizeCallback(GLFWwindow* window, int width,
                                       int height) {
  Window* wnd = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
  wnd->resized_ = true;
}

void Window::CursorPosCallback(GLFWwindow* window, double x, double y) {
  Window* wnd = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
  wnd->engine_->MousePosCallback(x, y);
}

}  // namespace Renderer