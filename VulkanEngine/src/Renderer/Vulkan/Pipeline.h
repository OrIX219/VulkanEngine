#pragma once

#include "vulkan/vulkan.hpp"

#include "LogicalDevice.h"
#include "RenderPass.h"
#include "Vertex.h"

namespace Renderer {

class Pipeline {
 public:
  Pipeline();
  ~Pipeline();

  VkResult Create(LogicalDevice* device,
                  VkGraphicsPipelineCreateInfo* create_info,
                  bool foreign_layout);
  void Destroy();

  VkPipeline Get() const;
  VkPipelineLayout GetLayout() const;

  void Bind(CommandBuffer command_buffer,
            VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS);

  friend bool operator==(const Pipeline& lhs, const Pipeline& rhs) {
    return lhs.pipeline_ == rhs.pipeline_;
  }
  friend bool operator!=(const Pipeline& lhs, const Pipeline& rhs) {
    return lhs.pipeline_ != rhs.pipeline_;
  }

 private:
  bool foreign_layout_;
  VkPipelineLayout pipeline_layout_;
  VkPipeline pipeline_;

  LogicalDevice* device_;
  RenderPass* render_pass_;
};

class PipelineBuilder {
 public: 
  PipelineBuilder();
  ~PipelineBuilder();

  static PipelineBuilder Begin(LogicalDevice* device);

  Pipeline Build(RenderPass& render_pass);

  PipelineBuilder& SetShaders(class ShaderEffect* effect); 
  PipelineBuilder& SetVertexInputDescription(
      VertexInputDescription vertex_input);
  PipelineBuilder& SetInputAssembly(
      VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      VkBool32 primitive_restart_enable = VK_FALSE);
  PipelineBuilder& SetInputAssembly(
      VkPipelineInputAssemblyStateCreateInfo* input_assembly);
  PipelineBuilder& SetViewport(VkViewport viewport);
  PipelineBuilder& SetScissors(VkRect2D scissors);
  PipelineBuilder& SetRasterizer(
      VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL, float line_width = 1.f,
      VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT,
      VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE,
      VkBool32 depth_bias_enable = VK_FALSE);
  PipelineBuilder& SetRasterizer(
      VkPipelineRasterizationStateCreateInfo* rasterizer);
  PipelineBuilder& SetColorBlendAttachment(VkBool32 blend_enable = VK_FALSE,
      VkColorComponentFlags color_write_mask = VK_COLOR_COMPONENT_R_BIT |
                                               VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT |
                                               VK_COLOR_COMPONENT_A_BIT);
  PipelineBuilder& SetColorBlendAttachment(
      VkPipelineColorBlendAttachmentState* color_blend_attachment);
  PipelineBuilder& SetMultisampling(
      VkSampleCountFlagBits rasterization_samples = VK_SAMPLE_COUNT_1_BIT,
      VkBool32 sample_shading_enable = VK_FALSE,
      float min_sample_shading = 0.2f);
  PipelineBuilder& SetMultisampling(
      VkPipelineMultisampleStateCreateInfo* multisampling);
  PipelineBuilder& SetDepthStencil(
      VkBool32 depth_test_enable = VK_TRUE,
      VkBool32 depth_write_enable = VK_TRUE,
      VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS,
      VkBool32 stencil_test_enable = VK_FALSE);
  PipelineBuilder& SetDepthStencil(
      VkPipelineDepthStencilStateCreateInfo* depth_stencil);
  PipelineBuilder& SetLayout(
      const std::vector<VkDescriptorSetLayout>& descriptor_layouts,
      const std::vector<VkPushConstantRange>& push_constants);
  PipelineBuilder& SetLayout(VkPipelineLayout layout);

  PipelineBuilder& SetDefaults();

 private:
  std::vector<VkPipelineShaderStageCreateInfo> shader_stages_;
  VkPipelineVertexInputStateCreateInfo vertex_input_info_;
  VertexInputDescription vertex_description_;
  VkPipelineInputAssemblyStateCreateInfo input_assembly_info_;
  std::optional<VkViewport> viewport_;
  std::optional<VkRect2D> scissors_;
  VkPipelineRasterizationStateCreateInfo rasterizer_;
  VkPipelineColorBlendAttachmentState color_blend_attachment_;
  VkPipelineMultisampleStateCreateInfo multisampling_;
  std::optional<VkPipelineDepthStencilStateCreateInfo> depth_stencil_;
  VkPipelineLayout pipeline_layout_;
  bool foreign_layout_;
  std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  std::vector<VkPushConstantRange> push_constants_;

  LogicalDevice* device_;
};

class ComputePipelineBuilder {
 public:
  ComputePipelineBuilder();
  ~ComputePipelineBuilder();

  static ComputePipelineBuilder Begin(LogicalDevice* device);

  ComputePipelineBuilder& SetShaderStage(
      const VkPipelineShaderStageCreateInfo& stage);
  ComputePipelineBuilder& SetLayout(VkPipelineLayout layout);

  VkPipeline Build();

 private:
  VkPipelineShaderStageCreateInfo shader_stage_;
  VkPipelineLayout pipeline_layout_;

  LogicalDevice* device_;
};

}