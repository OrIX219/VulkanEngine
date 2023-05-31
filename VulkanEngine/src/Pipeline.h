#pragma once

#include "vulkan/vulkan.hpp"

#include "LogicalDevice.h"
#include "RenderPass.h"
#include "Vertex.h"

namespace Renderer {

struct DescriptorSetLayout {
  DescriptorSetLayout() : layout(VK_NULL_HANDLE) {}

  void AddBinding(uint32_t binding, VkDescriptorType type,
                  VkShaderStageFlags stage, uint32_t descriptor_count = 1,
                  const VkSampler* immutable_samplers = nullptr) {
    bindings.push_back(
        {binding, type, descriptor_count, stage, immutable_samplers});
  }

  void AddBinding(VkDescriptorSetLayoutBinding binding) {
    bindings.push_back(std::move(binding));
  }

  VkResult Create(LogicalDevice* device) {
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    VkResult res = vkCreateDescriptorSetLayout(device->GetDevice(),
                                               &layout_info, nullptr, &layout);
    bindings.clear();
    return res;
  }

  VkDescriptorSetLayout layout;
  std::vector<VkDescriptorSetLayoutBinding> bindings;
};

class Pipeline {
 public:
  Pipeline();
  ~Pipeline();

  VkResult Create(LogicalDevice* device,
                  VkGraphicsPipelineCreateInfo* create_info,
                  bool foreign_layout);
  void Destroy();

  VkPipeline GetPipeline() const;
  VkPipelineLayout GetLayout() const;

  void Bind(CommandBuffer command_buffer,
            VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS);

 private:
  bool foreign_layout_;
  VkPipelineLayout pipeline_layout_;
  VkPipeline pipeline_;

  LogicalDevice* device_;
  RenderPass* render_pass_;
};

class PipelineBuilder {
 public: 
  PipelineBuilder(LogicalDevice* device);
  ~PipelineBuilder();

  Pipeline Build(RenderPass* render_pass);

  PipelineBuilder& SetVertexShader(const char* path);
  PipelineBuilder& SetVertexShader(VkShaderModule shader_module);
  PipelineBuilder& SetFragmentShader(const char* path);
  PipelineBuilder& SetFragmentShader(VkShaderModule shader_module);
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
      VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE);
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
  VkShaderModule CreateShaderModule(const std::vector<char>& code);

  VkShaderModule vertex_module_, fragment_module_;
  VkPipelineShaderStageCreateInfo vertex_shader_;
  VkPipelineShaderStageCreateInfo fragment_shader_;
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

}