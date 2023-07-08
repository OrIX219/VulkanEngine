#include "Framebuffer.h"

namespace Renderer {

Framebuffer::Framebuffer() {}

Framebuffer::~Framebuffer() {}

VkFramebuffer Framebuffer::Get() const { return framebuffer_; }

VkResult Framebuffer::Resize(VkExtent2D extent) {
  extent_ = extent;
  Destroy();
  return Create();
}

VkResult Framebuffer::Resize(VkExtent2D extent,
                             const std::vector<VkImageView>& attachments,
                             uint32_t layers) {
  attachments_ = attachments;
  extent_ = extent;
  layers_ = layers;
  Destroy();
  return Create();
}

VkResult Framebuffer::Create() {
  VkFramebufferCreateInfo framebuffer_info{};
  framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_info.renderPass = render_pass_->Get();
  framebuffer_info.attachmentCount =
      static_cast<uint32_t>(attachments_.size());
  framebuffer_info.pAttachments = attachments_.data();
  framebuffer_info.width = extent_.width;
  framebuffer_info.height = extent_.height;
  framebuffer_info.layers = layers_;

  return vkCreateFramebuffer(device_->Get(), &framebuffer_info, nullptr,
                             &framebuffer_);
}

VkResult Framebuffer::Create(LogicalDevice* device, RenderPass* render_pass,
                             VkExtent2D extent,
                             const std::vector<VkImageView>& attachments,
                             uint32_t layers) {
  device_ = device;
  render_pass_ = render_pass;
  extent_ = extent;
  layers_ = layers;
  attachments_ = attachments;
  return Create();
}

void Framebuffer::Destroy() {
  vkDestroyFramebuffer(device_->Get(), framebuffer_, nullptr);
}

}  // namespace Renderer