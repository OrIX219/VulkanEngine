#pragma once

#include "vulkan/vulkan.hpp"

#include <vector>

#include "Image.h"
#include "LogicalDevice.h"
#include "RenderPass.h"
#include "Swapchain.h"

namespace Renderer {

class Framebuffer {
 public:
  Framebuffer();
  ~Framebuffer();

  VkFramebuffer GetFramebuffer() const;

  VkResult Create(LogicalDevice* device, RenderPass* render_pass,
                  VkExtent2D extent,
                  const std::vector<VkImageView>& attachments);
  void Destroy();

  /*
  Destroy and create framebuffers with new size
  */
  VkResult Resize(VkExtent2D extent);
  /*
  Destroy and create framebuffers with new size and attachments
  - Used for handling window resize
  */
  VkResult Resize(VkExtent2D extent,
                  const std::vector<VkImageView>& attachments);

 private:
  VkResult Create();

  VkFramebuffer framebuffer_;

  LogicalDevice* device_;
  RenderPass* render_pass_;
  VkExtent2D extent_;
  std::vector<VkImageView> attachments_;
};

}