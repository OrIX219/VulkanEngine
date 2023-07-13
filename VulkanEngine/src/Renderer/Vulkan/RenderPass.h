#pragma once

#include "vulkan/vulkan.hpp"

#include "CommandBuffer.h"
#include "LogicalDevice.h"

#include <vector>
#include <optional>

namespace Renderer {

class RenderPass {
 public:
  struct BeginInfo {
    std::vector<VkClearValue> clear_values;
    VkFramebuffer framebuffer;
    VkRect2D render_area;
    VkSubpassContents subpass_contents = VK_SUBPASS_CONTENTS_INLINE;
  };

  RenderPass();
  ~RenderPass();

  VkResult Create(LogicalDevice* device, VkRenderPassCreateInfo2* create_info);
  void Destroy();

  VkRenderPass Get() const;

  uint32_t GetColorAttachmentCount() const;

  void Begin(CommandBuffer command_buffer, const BeginInfo& begin_info);
  void End(CommandBuffer command_buffer);

 private:
  VkRenderPass render_pass_;
  uint32_t color_attachment_count_;

  LogicalDevice* device_;
};

struct RenderPassAttachment {
  VkAttachmentDescription2 description;

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
  std::vector<VkAttachmentReference2> color_attachments;
  std::vector<VkAttachmentReference2> resolve_attachments;
  std::optional<VkAttachmentReference2> depth_stencil_attachment;
  std::optional<VkAttachmentReference2> depth_stencil_resolve_attachment;
  VkSubpassDescriptionDepthStencilResolve depth_stencil_resolve{};


  RenderPassSubpass& AddColorAttachmentRef(uint32_t attachment_index,
      VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  RenderPassSubpass& AddResolveAttachmentRef(
      uint32_t attachment_index,
      VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  RenderPassSubpass& SetDepthStencilAttachmentRef(uint32_t attachment_index,
      VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  RenderPassSubpass& SetDepthStencilResolveAttachmentRef(
      uint32_t attachment_index,
      VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
};

class RenderPassBuilder {
 public:
  RenderPassBuilder(LogicalDevice* device);

  RenderPass Build();
  void Clear();

  RenderPassBuilder& AddAttachment(const RenderPassAttachment& attachment);
  RenderPassBuilder& AddSubpass(
      const RenderPassSubpass& subpass,
      VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS);
  RenderPassBuilder& AddDependency(uint32_t src_subpass, uint32_t dst_subpass,
                                   VkPipelineStageFlags src_stage_mask,
                                   VkAccessFlags src_access_mask,
                                   VkPipelineStageFlags dst_stage_mask,
                                   VkAccessFlags dst_access_mask);

 private:
  std::vector<VkAttachmentDescription2> attachments_;
  std::vector<VkSubpassDescription2> subpasses_;
  std::vector<VkSubpassDependency2> subpass_dependencies_;

  LogicalDevice* device_;
};

}