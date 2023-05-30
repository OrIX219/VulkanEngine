#pragma once

#include "vulkan/vulkan.hpp"

#include <vector>

#include "Image.h"
#include "LogicalDevice.h"
#include "RenderPass.h"
#include "Swapchain.h"

namespace Renderer {

class SwapchainFramebuffers {
 public:
  SwapchainFramebuffers();
  ~SwapchainFramebuffers();

  const std::vector<VkFramebuffer>& GetFramebuffers() const;
  VkFramebuffer GetFramebuffer(size_t index) const;

  VkResult Create(Swapchain* swapchain, RenderPass* render_pass,
                  Image* depth_image = nullptr);
  void Destroy();
  VkResult Recreate();

 private:
  std::vector<VkFramebuffer> framebuffers_;

  Swapchain* swapchain_;
  RenderPass* render_pass_;
  Image* depth_image_;
};

}