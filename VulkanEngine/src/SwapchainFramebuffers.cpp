#include "SwapchainFramebuffers.h"

namespace Renderer {

SwapchainFramebuffers::SwapchainFramebuffers() {}

SwapchainFramebuffers::~SwapchainFramebuffers() {}

const std::vector<VkFramebuffer>& SwapchainFramebuffers::GetFramebuffers()
    const {
  return framebuffers_;
}

VkFramebuffer SwapchainFramebuffers::GetFramebuffer(size_t index) const {
  return framebuffers_.at(index);
}

VkResult SwapchainFramebuffers::Recreate() {
  Destroy();
  return Create(swapchain_, render_pass_, depth_image_);
}

VkResult SwapchainFramebuffers::Create(Swapchain* swapchain,
                                       RenderPass* render_pass,
                                       Image* depth_image) {
  swapchain_ = swapchain;
  render_pass_ = render_pass;
  depth_image_ = depth_image;

  const auto& image_views = swapchain_->GetImageViews();
  framebuffers_.resize(image_views.size());

  VkExtent2D swap_chain_extent = swapchain_->GetImageExtent();
  for (size_t i = 0; i < image_views.size(); ++i) {
    std::vector<VkImageView> attachments = {image_views[i]};
    if (depth_image_) attachments.push_back(depth_image_->GetView());

    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = render_pass_->GetRenderPass();
    framebuffer_info.attachmentCount =
        static_cast<uint32_t>(attachments.size());
    framebuffer_info.pAttachments = attachments.data();
    framebuffer_info.width = swap_chain_extent.width;
    framebuffer_info.height = swap_chain_extent.height;
    framebuffer_info.layers = 1;

    VkResult res =
        vkCreateFramebuffer(swapchain_->GetLogicalDevice()->GetDevice(),
                            &framebuffer_info, nullptr, &framebuffers_[i]);
    if (res != VK_SUCCESS) return res;
  }

  return VK_SUCCESS;
}

void SwapchainFramebuffers::Destroy() {
  for (auto framebuffer : framebuffers_)
    vkDestroyFramebuffer(swapchain_->GetLogicalDevice()->GetDevice(),
                         framebuffer,
                         nullptr);
}

}  // namespace Renderer