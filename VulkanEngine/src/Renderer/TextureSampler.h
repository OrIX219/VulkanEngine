#pragma once

#include <vulkan/vulkan.h>

#include "LogicalDevice.h"

namespace Renderer {

struct AddressMode {
  VkSamplerAddressMode u;
  VkSamplerAddressMode v;
  VkSamplerAddressMode w;
};

class TextureSampler {
 public:
  TextureSampler();
  TextureSampler(LogicalDevice* device);
  ~TextureSampler();

  void Create(LogicalDevice* device, float min_lod = 0.f,
              float max_lod = VK_LOD_CLAMP_NONE, const void* next = nullptr);
  void Destroy();

  VkSampler Get();

  VkFilter GetMagFilter() const;
  VkFilter GetMinFilter() const;
  AddressMode GetAddressMode() const;
  float GetAnisotropyEnable() const;

  TextureSampler& SetMagFilter(VkFilter filter = VK_FILTER_LINEAR);
  TextureSampler& SetMinFilter(VkFilter filter = VK_FILTER_LINEAR);
  TextureSampler& SetAddressMode(AddressMode address_mode);
  TextureSampler& SetAnisotropyEnable(bool enable = false);
  TextureSampler& SetMipmapMode(
      VkSamplerMipmapMode mode = VK_SAMPLER_MIPMAP_MODE_LINEAR);
  TextureSampler& SetCompare(bool compare_enable = VK_FALSE,
                             VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS);
  TextureSampler& SetBorderColor(
      VkBorderColor border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK);

  TextureSampler& SetDefaults();

 private:
  VkSampler sampler_;

  VkFilter mag_filter_;
  VkFilter min_filter_;
  AddressMode address_mode_;
  bool enable_anisotropy_;
  bool compare_enable_;
  VkCompareOp compare_op_;
  VkBorderColor border_color_;
  VkSamplerMipmapMode mipmap_mode_;

  LogicalDevice* device_;
};

}