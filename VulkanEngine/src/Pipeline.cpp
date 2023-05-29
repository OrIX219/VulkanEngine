#include "Pipeline.h"

#include <fstream>

namespace Renderer {

std::vector<char> ReadFileBinary(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) return {};

  size_t file_size = (size_t)file.tellg();
  std::vector<char> buffer(file_size);

  file.seekg(0);
  file.read(buffer.data(), file_size);

  return buffer;
}

Pipeline::Pipeline()
    : pipeline_(VK_NULL_HANDLE), pipeline_layout_(VK_NULL_HANDLE) {}

Pipeline::~Pipeline() {}

VkResult Pipeline::Create(LogicalDevice* device,
                          VkGraphicsPipelineCreateInfo* create_info,
                          bool foreign_layout) {
  device_ = device;
  pipeline_layout_ = create_info->layout;
  foreign_layout_ = foreign_layout;
  return vkCreateGraphicsPipelines(device->GetDevice(), VK_NULL_HANDLE, 1,
                                   create_info, nullptr, &pipeline_);
}

void Pipeline::Destroy() {
  if (pipeline_ == VK_NULL_HANDLE) return;

  vkDestroyPipeline(device_->GetDevice(), pipeline_, nullptr);
  if (!foreign_layout_)
    vkDestroyPipelineLayout(device_->GetDevice(), pipeline_layout_, nullptr);

  pipeline_ = VK_NULL_HANDLE;
}

VkPipeline Pipeline::GetPipeline() const { return pipeline_; }

VkPipelineLayout Pipeline::GetLayout() const { return pipeline_layout_; }

void Pipeline::Bind(CommandBuffer command_buffer,
                    VkPipelineBindPoint bind_point) {
  vkCmdBindPipeline(command_buffer.GetBuffer(), bind_point, pipeline_);
}

PipelineBuilder::PipelineBuilder(LogicalDevice* device)
    : device_(device),
      foreign_layout_(false),
      vertex_shader_{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
      fragment_shader_{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
      vertex_input_info_{
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO},
      input_assembly_info_{
          VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO},
      rasterizer_{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO},
      color_blend_attachment_{
          VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO},
      multisampling_{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO},
      pipeline_layout_{},
      vertex_module_(VK_NULL_HANDLE),
      fragment_module_(VK_NULL_HANDLE) {}

PipelineBuilder::~PipelineBuilder() {
  vkDestroyShaderModule(device_->GetDevice(), vertex_module_, nullptr);
  vkDestroyShaderModule(device_->GetDevice(), fragment_module_, nullptr);
}

Pipeline PipelineBuilder::Build(RenderPass* render_pass) {
  VkPipelineShaderStageCreateInfo shader_stages[] = {vertex_shader_,
                                                     fragment_shader_};

  std::vector<VkDynamicState> dynamic_states;
  if (!viewport_.has_value())
    dynamic_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  if (!scissors_.has_value())
    dynamic_states.push_back(VK_DYNAMIC_STATE_SCISSOR);

  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount =
      static_cast<uint32_t>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  if (viewport_.has_value()) viewport_state.pViewports = &viewport_.value();
  viewport_state.scissorCount = 1;
  if (scissors_.has_value()) viewport_state.pScissors = &scissors_.value();

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment_;

  if (!foreign_layout_) {
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount =
        static_cast<uint32_t>(descriptor_layouts_.size());
    if (descriptor_layouts_.size() > 0)
      pipeline_layout_info.pSetLayouts = descriptor_layouts_.data();
    pipeline_layout_info.pushConstantRangeCount =
        static_cast<uint32_t>(push_constants_.size());
    if (push_constants_.size() > 0)
      pipeline_layout_info.pPushConstantRanges = push_constants_.data();
    vkCreatePipelineLayout(device_->GetDevice(), &pipeline_layout_info, nullptr,
                           &pipeline_layout_);
  }

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input_info_;
  pipeline_info.pInputAssemblyState = &input_assembly_info_;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer_;
  pipeline_info.pMultisampleState = &multisampling_;
  if (depth_stencil_.has_value())
    pipeline_info.pDepthStencilState = &depth_stencil_.value();
  pipeline_info.pColorBlendState = &color_blending;
  if (dynamic_state.dynamicStateCount > 0)
    pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout_;
  pipeline_info.renderPass = render_pass->GetRenderPass();
  pipeline_info.subpass = 0;

  Pipeline pipeline;

  if (pipeline.Create(device_, &pipeline_info, foreign_layout_) != VK_SUCCESS) {
    std::cerr << "Failed to create pipeline!" << std::endl;
    return Pipeline();
  } else {
    return pipeline;
  }
}

PipelineBuilder& PipelineBuilder::SetVertexShader(const char* path) {
  if (vertex_module_ != VK_NULL_HANDLE)
    vkDestroyShaderModule(device_->GetDevice(), vertex_module_, nullptr);

  std::vector<char> shader_code = ReadFileBinary(path);
  vertex_module_ = CreateShaderModule(shader_code);

  vertex_shader_.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_shader_.module = vertex_module_;
  vertex_shader_.pName = "main";

  return *this;
}

PipelineBuilder& PipelineBuilder::SetVertexShader(
    VkShaderModule shader_module) {
  if (vertex_module_ != VK_NULL_HANDLE)
    vkDestroyShaderModule(device_->GetDevice(), vertex_module_, nullptr);

  vertex_module_ = VK_NULL_HANDLE;

  vertex_shader_.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_shader_.module = shader_module;
  vertex_shader_.pName = "main";

  return *this;
}

PipelineBuilder& PipelineBuilder::SetFragmentShader(const char* path) {
  if (fragment_module_ != VK_NULL_HANDLE)
    vkDestroyShaderModule(device_->GetDevice(), fragment_module_, nullptr);

  std::vector<char> shader_code = ReadFileBinary(path);
  fragment_module_ = CreateShaderModule(shader_code);

  fragment_shader_.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragment_shader_.module = fragment_module_;
  fragment_shader_.pName = "main";

  return *this;
}

PipelineBuilder& PipelineBuilder::SetFragmentShader(
    VkShaderModule shader_module) {
  if (fragment_module_ != VK_NULL_HANDLE)
    vkDestroyShaderModule(device_->GetDevice(), fragment_module_, nullptr);

  fragment_module_ = VK_NULL_HANDLE;

  fragment_shader_.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragment_shader_.module = shader_module;
  fragment_shader_.pName = "main";

  return *this;
}

PipelineBuilder& PipelineBuilder::SetVertexInputDescription(
    VertexInputDescription vertex_input) {
  vertex_description_ = vertex_input;
  vertex_input_info_.vertexBindingDescriptionCount =
      static_cast<uint32_t>(vertex_description_.binding_descriptions.size());
  vertex_input_info_.pVertexBindingDescriptions =
      vertex_description_.binding_descriptions.data();
  vertex_input_info_.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(vertex_description_.attribute_descriptions.size());
  vertex_input_info_.pVertexAttributeDescriptions =
      vertex_description_.attribute_descriptions.data();

  return *this;
}

PipelineBuilder& PipelineBuilder::SetInputAssembly(
    VkPrimitiveTopology topology, VkBool32 primitive_restart_enable) {
  input_assembly_info_.topology = topology;
  input_assembly_info_.primitiveRestartEnable = primitive_restart_enable;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetInputAssembly(
    VkPipelineInputAssemblyStateCreateInfo* input_assembly) {
  input_assembly_info_ = *input_assembly;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetViewport(VkViewport viewport) {
  viewport_ = viewport;
  
  return *this;
}

PipelineBuilder& PipelineBuilder::SetScissors(VkRect2D scissors) {
  scissors_ = scissors;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetRasterizer(VkPolygonMode polygon_mode,
                                                float line_width,
                                                VkCullModeFlags cull_mode,
                                                VkFrontFace front_face) {
  rasterizer_.polygonMode = polygon_mode;
  rasterizer_.lineWidth = line_width;
  rasterizer_.cullMode = cull_mode;
  rasterizer_.frontFace = front_face;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetRasterizer(
    VkPipelineRasterizationStateCreateInfo* rasterizer) {
  rasterizer_ = *rasterizer;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetColorBlendAttachment(
    VkBool32 blend_enable, VkColorComponentFlags color_write_mask) {
  color_blend_attachment_.blendEnable = blend_enable;
  color_blend_attachment_.colorWriteMask = color_write_mask;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetColorBlendAttachment(
    VkPipelineColorBlendAttachmentState* color_blend_attachment) {
  color_blend_attachment_ = *color_blend_attachment;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetMultisampling(
    VkBool32 sample_shading_enable,
    VkSampleCountFlagBits rasterization_samples) {
  multisampling_.sampleShadingEnable = sample_shading_enable;
  multisampling_.rasterizationSamples = rasterization_samples;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetMultisampling(
    VkPipelineMultisampleStateCreateInfo* multisampling) {
  multisampling_ = *multisampling;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthStencil(
    VkBool32 depth_test_enable, VkBool32 depth_write_enable,
    VkCompareOp depth_compare_op, VkBool32 stencil_test_enable) {
  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = depth_test_enable;
  depth_stencil.depthWriteEnable = depth_write_enable;
  depth_stencil.depthCompareOp = depth_compare_op;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = stencil_test_enable;

  depth_stencil_ = depth_stencil;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthStencil(
    VkPipelineDepthStencilStateCreateInfo* depth_stencil) {
  depth_stencil_ = *depth_stencil;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetLayout(
    const std::vector<VkDescriptorSetLayout>& descriptor_layouts,
    const std::vector<VkPushConstantRange>& push_constants) {
  descriptor_layouts_ = descriptor_layouts;
  push_constants_ = push_constants;
  foreign_layout_ = false;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetLayout(VkPipelineLayout layout) {
  foreign_layout_ = true;
  pipeline_layout_ = layout;

  return *this;
}

PipelineBuilder& PipelineBuilder::SetDefaults() {
  static const std::vector<VkDescriptorSetLayout> empty_layouts;
  static const std::vector<VkPushConstantRange> empty_push_constants;
  SetInputAssembly()
      .SetRasterizer()
      .SetColorBlendAttachment()
      .SetMultisampling()
      .SetLayout(empty_layouts, empty_push_constants);

  return *this;
}

VkShaderModule PipelineBuilder::CreateShaderModule(
    const std::vector<char>& code) {
  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = code.size();
  create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule shader_module;
  if (vkCreateShaderModule(device_->GetDevice(), &create_info, nullptr,
                           &shader_module) != VK_SUCCESS) {
    std::cerr << "Failed to create shader module!" << std::endl;
    return VK_NULL_HANDLE;
  }

  return shader_module;
}

}  // namespace Renderer