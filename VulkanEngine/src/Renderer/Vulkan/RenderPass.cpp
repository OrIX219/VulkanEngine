#include "RenderPass.h"

namespace Renderer {

RenderPass::RenderPass() {}

RenderPass::~RenderPass() {}

VkResult RenderPass::Create(LogicalDevice* device,
                            VkRenderPassCreateInfo2* create_info) {
  device_ = device;

  return vkCreateRenderPass2(device_->Get(), create_info, nullptr,
                             &render_pass_);
}

void RenderPass::Destroy() {
  vkDestroyRenderPass(device_->Get(), render_pass_, nullptr);
}

VkRenderPass RenderPass::Get() const { return render_pass_; }

void RenderPass::Begin(CommandBuffer command_buffer,
                       const BeginInfo& begin_info) {
  VkRenderPassBeginInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.renderPass = render_pass_;
  render_pass_info.renderArea = begin_info.render_area;
  render_pass_info.framebuffer = begin_info.framebuffer;
  render_pass_info.clearValueCount =
      static_cast<uint32_t>(begin_info.clear_values.size());
  render_pass_info.pClearValues = begin_info.clear_values.data();

  vkCmdBeginRenderPass(command_buffer.Get(), &render_pass_info,
                       begin_info.subpass_contents);
}

void RenderPass::End(CommandBuffer command_buffer) {
  vkCmdEndRenderPass(command_buffer.Get());
}

RenderPassAttachment::RenderPassAttachment()
    : description{VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2} {}

RenderPassAttachment& RenderPassAttachment::SetFormat(VkFormat format) {
  description.format = format;
  return *this;
}

RenderPassAttachment& RenderPassAttachment::SetLayouts(
    VkImageLayout initial_layout, VkImageLayout final_layout) {
  description.initialLayout = initial_layout;
  description.finalLayout = final_layout;
  return *this;
}

RenderPassAttachment& RenderPassAttachment::SetOperations(
    VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op) {
  description.loadOp = load_op;
  description.storeOp = store_op;
  return *this;
}

RenderPassAttachment& RenderPassAttachment::SetSamples(
    VkSampleCountFlagBits samples) {
  description.samples = samples;
  return *this;
}

RenderPassAttachment& RenderPassAttachment::SetDefaults() { 
  SetLayouts().SetOperations().SetSamples();
  return *this; 
}

RenderPassSubpass& RenderPassSubpass::AddColorAttachmentRef(
    uint32_t attachment_index, VkImageLayout layout) {
  VkAttachmentReference2 attachment{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
  attachment.attachment = attachment_index;
  attachment.layout = layout;
  color_attachments.push_back(attachment);
  
  return *this;
}

RenderPassSubpass& RenderPassSubpass::AddResolveAttachmentRef(
    uint32_t attachment_index, VkImageLayout layout) {
  VkAttachmentReference2 attachment{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
  attachment.attachment = attachment_index;
  attachment.layout = layout;
  resolve_attachments.push_back(attachment);

  return *this;
}

RenderPassSubpass& RenderPassSubpass::SetDepthStencilAttachmentRef(
    uint32_t attachment_index, VkImageLayout layout) {
  VkAttachmentReference2 attachment{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
  attachment.attachment = attachment_index;
  attachment.layout = layout;
  depth_stencil_attachment = attachment;
  
  return *this;
}

RenderPassSubpass& RenderPassSubpass::SetDepthStencilResolveAttachmentRef(
    uint32_t attachment_index, VkImageLayout layout) {
  VkAttachmentReference2 attachment{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
  attachment.attachment = attachment_index;
  attachment.layout = layout;
  depth_stencil_resolve_attachment = attachment;

  depth_stencil_resolve.sType =
      VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
  depth_stencil_resolve.depthResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
  depth_stencil_resolve.stencilResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
  depth_stencil_resolve.pDepthStencilResolveAttachment =
      &depth_stencil_resolve_attachment.value();

  return *this;
}

RenderPass RenderPassBuilder::Build() {
  VkRenderPassCreateInfo2 render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
  render_pass_info.attachmentCount = static_cast<uint32_t>(attachments_.size());
  render_pass_info.pAttachments = attachments_.data();
  render_pass_info.subpassCount = static_cast<uint32_t>(subpasses_.size());
  render_pass_info.pSubpasses = subpasses_.data();
  render_pass_info.dependencyCount =
      static_cast<uint32_t>(subpass_dependencies_.size());
  render_pass_info.pDependencies = subpass_dependencies_.data();

  RenderPass render_pass;
  render_pass.Create(device_, &render_pass_info);
  return render_pass;
}

void RenderPassBuilder::Clear() {
  attachments_.clear();
  subpasses_.clear();
  subpass_dependencies_.clear();
}

RenderPassBuilder::RenderPassBuilder(LogicalDevice* device) : device_(device) {}

RenderPassBuilder& RenderPassBuilder::AddAttachment(
    const RenderPassAttachment& attachment) {
  attachments_.push_back(attachment.description);

  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddSubpass(
    const RenderPassSubpass& subpass, VkPipelineBindPoint bind_point) {
  VkSubpassDescription2 subpass_info{VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2};
  subpass_info.pipelineBindPoint = bind_point;
  subpass_info.colorAttachmentCount =
      static_cast<uint32_t>(subpass.color_attachments.size());
  subpass_info.pColorAttachments = subpass.color_attachments.data();
  if (subpass.resolve_attachments.size())
    subpass_info.pResolveAttachments = subpass.resolve_attachments.data();
  if (subpass.depth_stencil_attachment.has_value())
    subpass_info.pDepthStencilAttachment = &subpass.depth_stencil_attachment.value();
  if (subpass.depth_stencil_resolve_attachment.has_value())
    subpass_info.pNext = &subpass.depth_stencil_resolve;

  subpasses_.push_back(subpass_info);

  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddDependency(
    uint32_t src_subpass, uint32_t dst_subpass,
    VkPipelineStageFlags src_stage_mask, VkAccessFlags src_access_mask,
    VkPipelineStageFlags dst_stage_mask, VkAccessFlags dst_access_mask) {
  VkSubpassDependency2 dependency{VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2};
  dependency.srcSubpass = src_subpass;
  dependency.dstSubpass = dst_subpass;
  dependency.srcStageMask = src_stage_mask;
  dependency.srcAccessMask = src_access_mask;
  dependency.dstStageMask = dst_stage_mask;
  dependency.dstAccessMask = dst_access_mask;
  subpass_dependencies_.push_back(dependency);

  return *this;
}

}  // namespace Renderer