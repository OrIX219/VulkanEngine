#pragma once

#include "vulkan/vulkan.hpp"

#include "CommandBuffer.h"
#include "LogicalDevice.h"

#include <vector>
#include <optional>

namespace Renderer {

class RenderPass {
 public:
  RenderPass();
  ~RenderPass();

  VkResult CreateDefault(LogicalDevice* device, VkFormat image_format);
  VkResult Create(LogicalDevice* device, VkRenderPassCreateInfo* create_info);
  void Destroy();

  VkRenderPass GetRenderPass() const;

  void Begin(CommandBuffer command_buffer, VkFramebuffer framebuffer,
             VkRect2D render_area,
             const std::vector<VkClearValue>& clear_values,
             VkSubpassContents subpass_contents = VK_SUBPASS_CONTENTS_INLINE);
  void End(CommandBuffer command_buffer);

 private:
  VkRenderPass render_pass_;

  LogicalDevice* device_;
};

struct RenderPassAttachment {
  VkAttachmentDescription description;

  RenderPassAttachment();

  RenderPassAttachment& SetFormat(VkFormat format);
  RenderPassAttachment& SetLayouts(
      VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
      VkImageLayout final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  RenderPassAttachment& SetOperations(
      VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
      VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE);
  RenderPassAttachment& SetSamples(
      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

  RenderPassAttachment& SetDefaults();
};

struct RenderPassSubpass {
  std::vector<VkAttachmentReference> color_attachments;
  std::optional<VkAttachmentReference> depth_stencil_attachment;

  RenderPassSubpass& AddColorAttachmentRef(uint32_t attachment_index,
      VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  RenderPassSubpass& SetDepthStencilAttachmentRef(uint32_t attachment_index,
      VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
};

class RenderPassBuilder {
 public:
  RenderPassBuilder(LogicalDevice* device);

  RenderPass Build();

  RenderPassBuilder& AddAttachment(RenderPassAttachment* attachment);
  RenderPassBuilder& AddSubpass(
      RenderPassSubpass* subpass,
      VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS);
  RenderPassBuilder& AddDependency(uint32_t src_subpass, uint32_t dst_subpass,
                                   VkPipelineStageFlags src_stage_mask,
                                   VkAccessFlags src_access_mask,
                                   VkPipelineStageFlags dst_stage_mask,
                                   VkAccessFlags dst_access_mask);

 private:
  std::vector<VkAttachmentDescription> attachments_;
  std::vector<VkSubpassDescription> subpasses_;
  std::vector<VkSubpassDependency> subpass_dependencies_;

  LogicalDevice* device_;
};

}