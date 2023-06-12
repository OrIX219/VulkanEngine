#include "RenderPass.h"

namespace Renderer {

RenderPass::RenderPass() {}

RenderPass::~RenderPass() {}

VkResult RenderPass::CreateDefault(LogicalDevice* device,
                                   VkFormat image_format) {
  device_ = device;

  VkAttachmentDescription color_attachment{};
  color_attachment.format = image_format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depth_attachment{};
  depth_attachment.format = VK_FORMAT_D32_SFLOAT;
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref{};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkSubpassDependency depth_dependency{};
  depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  depth_dependency.dstSubpass = 0;
  depth_dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  depth_dependency.srcAccessMask = 0;
  depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  std::array<VkSubpassDependency, 2> dependencies = {dependency, depth_dependency};

  std::array<VkAttachmentDescription, 2> attachments = {color_attachment, depth_attachment};
  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
  render_pass_info.pAttachments = attachments.data();
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
  render_pass_info.pDependencies = dependencies.data();

  return vkCreateRenderPass(device_->GetDevice(), &render_pass_info, nullptr,
                            &render_pass_);
}

VkResult RenderPass::Create(LogicalDevice* device,
                            VkRenderPassCreateInfo* create_info) {
  device_ = device;

  return vkCreateRenderPass(device_->GetDevice(), create_info, nullptr,
                            &render_pass_);
}

void RenderPass::Destroy() {
  vkDestroyRenderPass(device_->GetDevice(), render_pass_, nullptr);
}

VkRenderPass RenderPass::GetRenderPass() const { return render_pass_; }

void RenderPass::Begin(CommandBuffer command_buffer, VkFramebuffer framebuffer,
                       VkRect2D render_area,
                       const std::vector<VkClearValue>& clear_values,
                       VkSubpassContents subpass_contents) {
  VkRenderPassBeginInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.renderPass = render_pass_;
  render_pass_info.renderArea = std::move(render_area);
  render_pass_info.framebuffer = framebuffer;
  render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
  render_pass_info.pClearValues = clear_values.data();

  vkCmdBeginRenderPass(command_buffer.GetBuffer(), &render_pass_info,
                       subpass_contents);
}

void RenderPass::End(CommandBuffer command_buffer) {
  vkCmdEndRenderPass(command_buffer.GetBuffer());
}

RenderPassAttachment::RenderPassAttachment() : description{0} {}

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
  VkAttachmentReference attachment{};
  attachment.attachment = attachment_index;
  attachment.layout = layout;
  color_attachments.push_back(attachment);
  
  return *this;
}

RenderPassSubpass& RenderPassSubpass::AddResolveAttachmentRef(
    uint32_t attachment_index, VkImageLayout layout) {
  VkAttachmentReference attachment{};
  attachment.attachment = attachment_index;
  attachment.layout = layout;
  resolve_attachments.push_back(attachment);

  return *this;
}

RenderPassSubpass& RenderPassSubpass::SetDepthStencilAttachmentRef(
    uint32_t attachment_index, VkImageLayout layout) {
  depth_stencil_attachment = {attachment_index, layout};
  
  return *this;
}

RenderPass RenderPassBuilder::Build() {
  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
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

RenderPassBuilder::RenderPassBuilder(LogicalDevice* device) : device_(device) {}

RenderPassBuilder& RenderPassBuilder::AddAttachment(
    RenderPassAttachment* attachment) {
  attachments_.push_back(attachment->description);

  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddSubpass(
    RenderPassSubpass* subpass, VkPipelineBindPoint bind_point) {
  VkSubpassDescription subpass_info{};
  subpass_info.pipelineBindPoint = bind_point;
  subpass_info.colorAttachmentCount =
      static_cast<uint32_t>(subpass->color_attachments.size());
  subpass_info.pColorAttachments = subpass->color_attachments.data();
  if (subpass->resolve_attachments.size())
    subpass_info.pResolveAttachments = subpass->resolve_attachments.data();
  if (subpass->depth_stencil_attachment.has_value())
    subpass_info.pDepthStencilAttachment = &subpass->depth_stencil_attachment.value();
  subpasses_.push_back(subpass_info);

  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddDependency(
    uint32_t src_subpass, uint32_t dst_subpass,
    VkPipelineStageFlags src_stage_mask, VkAccessFlags src_access_mask,
    VkPipelineStageFlags dst_stage_mask, VkAccessFlags dst_access_mask) {
  VkSubpassDependency dependency{};
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