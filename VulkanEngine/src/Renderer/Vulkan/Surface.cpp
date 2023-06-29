#include "Surface.h"

namespace Renderer {

Surface::Surface() : surface_(VK_NULL_HANDLE) {}

Surface::~Surface() {}

VkResult Surface::Init(VulkanInstance* instance, Window* window) {
  instance_ = instance;
  window_ = window;

  return glfwCreateWindowSurface(instance_->Get(), window->GetWindow(),
                                 nullptr, &surface_);
}

void Surface::Destroy() {
  vkDestroySurfaceKHR(instance_->Get(), surface_, nullptr);
}

VkSurfaceKHR Surface::Get() { return surface_; }

Window* Surface::GetWindow() { return window_; }

}  // namespace Renderer