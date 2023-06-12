#include "Shaders.h"

#include <fstream>
#include <sstream>
#include <iostream>

#include <spirv_reflect/spirv_reflect.h>

#include "StringHash.h"
#include "Logger.h"

namespace Renderer {

bool LoadShaderModule(LogicalDevice* device, const char* path,
                      ShaderModule* module) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);

  if (!file.is_open()) return false;

  size_t file_size = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(file_size);

  file.seekg(0);
  file.read(buffer.data(), file_size);

  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = buffer.size();
  create_info.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

  VkShaderModule shader_module;
  if (vkCreateShaderModule(device->GetDevice(), &create_info, nullptr,
                           &shader_module) != VK_SUCCESS)
    return false;

  module->code = std::move(buffer);
  module->module = shader_module;

  return true;
}

uint32_t HashDescriptorLayoutInfo(VkDescriptorSetLayoutCreateInfo* info) {
  std::stringstream stream;

  stream << info->flags;
  stream << info->bindingCount;

  for (uint32_t i = 0; i < info->bindingCount; ++i) {
    const VkDescriptorSetLayoutBinding& binding = info->pBindings[i];
    stream << binding.binding;
    stream << binding.descriptorCount;
    stream << binding.descriptorType;
    stream << binding.stageFlags;
  }

  std::string str = stream.str();

  return fnv1a_32(str.c_str(), str.length());
}

void ShaderEffect::AddStage(ShaderModule* module, VkShaderStageFlagBits stage) {
  stages_.push_back({module, stage});
}

struct DescriptorSetLayoutData {
  uint32_t set_number;
  VkDescriptorSetLayoutCreateInfo create_info{};
  std::vector<VkDescriptorSetLayoutBinding> bindings;
};
void ShaderEffect::ReflectLayout(LogicalDevice* device,
                                 ReflectionOverrides* overrides,
                                 size_t override_count) {
  std::vector<DescriptorSetLayoutData> layouts;
  std::vector<VkPushConstantRange> constant_ranges;

  for (ShaderEffect::ShaderStage& stage : stages_) {
    SpvReflectShaderModule spv_module;
    SpvReflectResult result = spvReflectCreateShaderModule(
        stage.shader_module->code.size(), stage.shader_module->code.data(),
        &spv_module);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
      LOG_FATAL("Failed to reflect shader module");

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&spv_module, &count, nullptr);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
      LOG_FATAL("Failed to reflect shader module");

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(&spv_module, &count, sets.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
      LOG_FATAL("Failed to reflect shader module");

    for (size_t i = 0; i < sets.size(); ++i) {
      const SpvReflectDescriptorSet& set = *sets[i];

      DescriptorSetLayoutData layout{};

      layout.bindings.resize(set.binding_count);
      for (size_t j = 0; j < set.binding_count; ++j) {
        const SpvReflectDescriptorBinding& binding = *set.bindings[j];
        VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[j];
        layout_binding.binding = binding.binding;
        layout_binding.descriptorType =
            static_cast<VkDescriptorType>(binding.descriptor_type);

        for (size_t override_idx = 0; override_idx < override_count; ++override_idx) {
          if (strcmp(binding.name, overrides[override_idx].name) == 0)
            layout_binding.descriptorType =
                overrides[override_idx].overriden_type;
        }

        layout_binding.descriptorCount = 1;
        for (size_t dim = 0; dim < binding.array.dims_count; ++dim) {
          layout_binding.descriptorCount *= binding.array.dims[dim];
        }

        layout_binding.stageFlags =
            static_cast<VkShaderStageFlagBits>(spv_module.shader_stage);

        ReflectedBinding reflected;
        reflected.binding = layout_binding.binding;
        reflected.set = set.set;
        reflected.type = layout_binding.descriptorType;

        bindings[binding.name] = reflected;
      }

      layout.set_number = set.set;
      layout.create_info.sType =
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      layout.create_info.bindingCount = set.binding_count;
      layout.create_info.pBindings = layout.bindings.data();

      layouts.push_back(layout);
    }

    result =
        spvReflectEnumeratePushConstantBlocks(&spv_module, &count, nullptr);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
      LOG_FATAL("Failed to reflect shader module");

    std::vector<SpvReflectBlockVariable*> push_constants(count);
    result =
        spvReflectEnumeratePushConstantBlocks(&spv_module, &count, push_constants.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
      LOG_FATAL("Failed to reflect shader module");

    if (count > 0) {
      VkPushConstantRange range{};
      range.offset = push_constants[0]->offset;
      range.size = push_constants[0]->size;
      range.stageFlags = stage.stage;
      constant_ranges.push_back(range);
    }
  }

  std::array<DescriptorSetLayoutData, 4> merged_layouts;
  for (uint32_t i = 0; i < 4; ++i) {
    DescriptorSetLayoutData& layout = merged_layouts[i];
    layout.set_number = i;
    layout.create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> binds;
    for (DescriptorSetLayoutData& layout_data : layouts) {
      if (layout_data.set_number == i) {
        for (VkDescriptorSetLayoutBinding& bind : layout_data.bindings) {
          auto iter = binds.find(bind.binding);
          if (iter == binds.end())
            binds[bind.binding] = bind;
          else
            binds[bind.binding].stageFlags |= bind.stageFlags;
        }
      }
    }
    for (auto& [key, value] : binds) {
      layout.bindings.push_back(value);
    }

    std::sort(layout.bindings.begin(), layout.bindings.end(),
              [](VkDescriptorSetLayoutBinding& lhs,
                 VkDescriptorSetLayoutBinding& rhs) {
                return lhs.binding < rhs.binding;
              });

    layout.create_info.bindingCount =
        static_cast<uint32_t>(layout.bindings.size());
    layout.create_info.pBindings = layout.bindings.data();

    if (layout.create_info.bindingCount > 0) {
      set_hashes[i] = HashDescriptorLayoutInfo(&layout.create_info);
      
      vkCreateDescriptorSetLayout(device->GetDevice(), &layout.create_info,
                                  nullptr, &set_layouts[i]);
    } else {
      set_hashes[i] = 0;
      set_layouts[i] = VK_NULL_HANDLE;
    }
  }

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.pushConstantRangeCount =
      static_cast<uint32_t>(constant_ranges.size());
  pipeline_layout_info.pPushConstantRanges = constant_ranges.data();

  std::array<VkDescriptorSetLayout, 4> compacted_layouts;
  uint32_t s = 0;
  for (size_t i = 0; i < 4; ++i) {
    if (set_layouts[i] != VK_NULL_HANDLE) {
      compacted_layouts[s] = set_layouts[i];
      ++s;
    }
  }

  pipeline_layout_info.setLayoutCount = s;
  pipeline_layout_info.pSetLayouts = compacted_layouts.data();

  vkCreatePipelineLayout(device->GetDevice(), &pipeline_layout_info, nullptr,
                         &built_layout);
}

void ShaderEffect::FillStages(
    std::vector<VkPipelineShaderStageCreateInfo>& pipeline_stages) {
  for (ShaderEffect::ShaderStage& stage : stages_) {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage.stage;
    info.module = stage.shader_module->module;
    info.pName = "main";
    pipeline_stages.push_back(info);
  }
}

ShaderDescriptorBinder::ShaderDescriptorBinder() : shaders_(nullptr) {}

void ShaderDescriptorBinder::BindBuffer(
    const char* name, const VkDescriptorBufferInfo& buffer_info) {
  BindDynamicBuffer(name, -1, buffer_info);
}

void ShaderDescriptorBinder::BindDynamicBuffer(
    const char* name, uint32_t offset,
    const VkDescriptorBufferInfo& buffer_info) {
  auto iter = shaders_->bindings.find(name);
  if (iter == shaders_->bindings.end()) return;

  const ShaderEffect::ReflectedBinding& bind = iter->second;

  for (auto& write : buffer_writes_) {
    if (bind.binding == write.dst_binding && bind.set == write.dst_set) {
      if (write.buffer_info.buffer != buffer_info.buffer ||
          write.buffer_info.offset != buffer_info.offset ||
          write.buffer_info.range != buffer_info.range) {
        write.buffer_info = buffer_info;
        write.dynamic_offset = offset;
        cached_descriptor_sets[write.dst_set] = VK_NULL_HANDLE;
      } else {
        write.dynamic_offset = offset;
      }
      return;
    }
  }

  BufferWriteDescriptor write;
  write.dst_set = bind.set;
  write.dst_binding = bind.binding;
  write.descriptor_type = bind.type;
  write.buffer_info = buffer_info;
  write.dynamic_offset = offset;

  cached_descriptor_sets[bind.set] = VK_NULL_HANDLE;

  buffer_writes_.push_back(write);
}

void ShaderDescriptorBinder::ApplyBinds(CommandBuffer command_buffer) {
  for (uint32_t i = 0; i < 2; ++i) {
    if (cached_descriptor_sets[i] == VK_NULL_HANDLE) continue;
    vkCmdBindDescriptorSets(
        command_buffer.GetBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
        shaders_->built_layout, i, 1, &cached_descriptor_sets[i],
        set_offsets_[i].count, set_offsets_[i].offsets.data());
  }
}

void ShaderDescriptorBinder::BuildSets(LogicalDevice* device,
                                       DescriptorAllocator& allocator) {
  std::array<std::vector<VkWriteDescriptorSet>, 4> writes{};

  std::sort(buffer_writes_.begin(), buffer_writes_.end(),
            [](BufferWriteDescriptor& lhs, BufferWriteDescriptor& rhs) {
              return (lhs.dst_set == rhs.dst_set
                          ? lhs.dst_binding < rhs.dst_binding
                          : lhs.dst_set < rhs.dst_set);
            });

  for (DynamicOffets& offset : set_offsets_)
    offset.count = 0;

  for (BufferWriteDescriptor& write : buffer_writes_) {
    uint32_t set = write.dst_set;
    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.descriptorType = write.descriptor_type;
    w.descriptorCount = 1;
    w.dstBinding = write.dst_binding;
    w.pBufferInfo = &write.buffer_info;

    writes[set].push_back(w);

    if (w.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
        w.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
      DynamicOffets& offset_set = set_offsets_[set];
      offset_set.offsets[offset_set.count] = write.dynamic_offset;
      ++offset_set.count;
    }
  }

  for (size_t i = 0; i < 4; ++i) {
    if (writes[i].size() == 0) continue;
    if (cached_descriptor_sets[i] != VK_NULL_HANDLE) continue;
    VkDescriptorSetLayout layout = shaders_->set_layouts[i];
    VkDescriptorSet descriptor;
    allocator.Allocate(&descriptor, layout);

    for (VkWriteDescriptorSet& write : writes[i]) write.dstSet = descriptor;

    vkUpdateDescriptorSets(device->GetDevice(),
                           static_cast<uint32_t>(writes[i].size()),
                           writes[i].data(), 0, nullptr);

    cached_descriptor_sets[i] = descriptor;
  }
}

void ShaderDescriptorBinder::SetShader(ShaderEffect* shader) {
  if (shaders_ && shaders_ != shader) {
    for (size_t i = 0; i < 4; ++i) {
      if (shader->set_hashes[i] != shaders_->set_hashes[i])
        cached_descriptor_sets[i] = VK_NULL_HANDLE;
      else if (shader->set_hashes[i] == 0)
        cached_descriptor_sets[i] = VK_NULL_HANDLE;
    }
  } else {
    for (size_t i = 0; i < 4; ++i)
      cached_descriptor_sets[i] = VK_NULL_HANDLE;
  }

  shaders_ = shader;
}

void ShaderCache::Init(LogicalDevice* device) { device_ = device; }

void ShaderCache::Destroy() {
  for (auto& module : module_cache_)
    vkDestroyShaderModule(device_->GetDevice(), module.second.module, nullptr);
}

ShaderModule* ShaderCache::GetShader(const std::string& path) {
  if (module_cache_.count(path) == 0) {
    ShaderModule shader;

    bool result = LoadShaderModule(device_, path.c_str(), &shader);
    if (!result) {
      LOG_ERROR("Failed to compile shader '{}'", path.c_str());
      return nullptr;
    }

    module_cache_[path] = shader;
  }

  return &module_cache_[path];
}

}  // namespace Renderer