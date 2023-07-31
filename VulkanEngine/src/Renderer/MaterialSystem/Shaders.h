#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <vector>
#include <unordered_map>
#include <string>

#include "CommandBuffer.h"
#include "Descriptors.h"
#include "LogicalDevice.h"

namespace Renderer {

struct ShaderModule {
  std::vector<char> code;
  VkShaderModule module;
};

bool LoadShaderModule(LogicalDevice* device, const char* path,
                      ShaderModule* module);

uint32_t HashDescriptorLayoutInfo(VkDescriptorSetLayoutCreateInfo* info);

class ShaderEffect {
 public:
  struct ReflectionOverrides {
    const char* name;
    VkDescriptorType overriden_type;
  };

  ~ShaderEffect();

  void Destroy();

  void AddStage(ShaderModule* module, VkShaderStageFlagBits stage,
                VkSpecializationInfo constants = {});

  void ReflectLayout(LogicalDevice* device,
                     ReflectionOverrides* overrides = nullptr,
                     size_t override_count = 0);

  void FillStages(
      std::vector<VkPipelineShaderStageCreateInfo>& pipeline_stages);
  VkPipelineLayout built_layout;

  struct ReflectedBinding {
    uint32_t set;
    uint32_t binding;
    VkDescriptorType type;
  };
  std::unordered_map<std::string, ReflectedBinding> bindings;
  std::array<VkDescriptorSetLayout, 4> set_layouts;
  std::array<uint32_t, 4> set_hashes;

 private:
  struct ShaderStage {
    ShaderModule* shader_module;
    VkShaderStageFlagBits stage;
    VkSpecializationInfo constants;
  };

  std::vector<ShaderStage> stages_;

  LogicalDevice* device_;
};

class ShaderDescriptorBinder {
 public:
  ShaderDescriptorBinder();

  struct BufferWriteDescriptor {
    uint32_t dst_set;
    uint32_t dst_binding;
    VkDescriptorType descriptor_type;
    VkDescriptorBufferInfo buffer_info;
    uint32_t dynamic_offset;
  };

  void BindBuffer(const char* name, const VkDescriptorBufferInfo& buffer_info);

  void BindDynamicBuffer(const char* name, uint32_t offset,
                         const VkDescriptorBufferInfo& buffer_info);

  void ApplyBinds(CommandBuffer command_buffer);

  void BuildSets(LogicalDevice* device, DescriptorAllocator& allocator);

  void SetShader(ShaderEffect* shader);

  std::array<VkDescriptorSet, 4> cached_descriptor_sets;

 private:
  struct DynamicOffets {
    std::array<uint32_t, 16> offsets;
    uint32_t count{0};
  };
  std::array<DynamicOffets, 4> set_offsets_;

  ShaderEffect* shaders_;
  std::vector<BufferWriteDescriptor> buffer_writes_;
};

class ShaderCache {
 public:
  void Init(LogicalDevice* device);
  void Destroy();

  ShaderModule* GetShader(const std::string& path);

 private:
  LogicalDevice* device_;
  std::unordered_map<std::string, ShaderModule> module_cache_;
};

}