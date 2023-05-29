#include "Surface.h"

namespace Renderer {

Surface::Surface() : surface_(VK_NULL_HANDLE) {}

Surface::~Surface() {}

VkResult Surface::Init(VulkanInstance* instance, Window* window) {
  instance_ = instance;
  window_ = window;

  return glfwCreateWindowSurface(instance_->GetInstance(), window->GetWindow(),
                                 nullptr, &surface_);
}

void Surface::Destroy() {
  vkDestroySurfaceKHR(instance_->GetInstance(), surface_, nullptr);
}

VkSurfaceKHR Surface::GetSurface() { return surface_; }

Window* Surface::GetWindow() { return window_; }

}  // namespace Renderer