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
              float max_lod = VK_LOD_CLAMP_NONE);
  void Destroy();

  VkSampler GetSampler();

  VkFilter GetMagFilter() const;
  VkFilter GetMinFilter() const;
  AddressMode GetAddressMode() const;
  float GetAnisotropyEnable() const;

  TextureSampler& SetMagFilter(VkFilter filter);
  TextureSampler& SetMinFilter(VkFilter filter);
  TextureSampler& SetAddressMode(AddressMode address_mode);
  TextureSampler& SetAnisotropyEnable(bool enable);

  TextureSampler& SetDefaults();

 private:
  VkSampler sampler_;

  VkFilter mag_filter_;
  VkFilter min_filter_;
  AddressMode address_mode_;
  bool enable_anisotropy_;

  LogicalDevice* device_;
};

}