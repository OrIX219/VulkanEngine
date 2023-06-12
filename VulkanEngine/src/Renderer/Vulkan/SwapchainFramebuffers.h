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
                  Image* color_image = nullptr,
                  Image* depth_image = nullptr);
  void Destroy();

  /*
  Destroy and create framebuffers for same swapchain
  - Used for handling window resize (must recreate swapchain first)
  */
  VkResult Recreate();

 private:
  std::vector<VkFramebuffer> framebuffers_;

  Swapchain* swapchain_;
  RenderPass* render_pass_;
  Image* color_image_;
  Image* depth_image_;
};

}