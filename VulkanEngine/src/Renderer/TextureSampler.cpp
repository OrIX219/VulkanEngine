#include "TextureSampler.h"
#include "Logger.h"

namespace Renderer {
TextureSampler::TextureSampler() : sampler_(VK_NULL_HANDLE) {}
TextureSampler::TextureSampler(LogicalDevice* device)
    : sampler_(VK_NULL_HANDLE),
      mag_filter_(VK_FILTER_LINEAR),
      min_filter_(VK_FILTER_LINEAR),
      address_mode_{VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    VK_SAMPLER_ADDRESS_MODE_REPEAT},
      enable_anisotropy_(false) {
  Create(device);
}

TextureSampler::~TextureSampler() {}

void TextureSampler::Create(LogicalDevice* device, float min_lod, float max_lod,
                            const void* next) {
  device_ = device;

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = mag_filter_;
  sampler_info.minFilter = min_filter_;
  sampler_info.addressModeU = address_mode_.u;
  sampler_info.addressModeV = address_mode_.v;
  sampler_info.addressModeW = address_mode_.w;
  sampler_info.anisotropyEnable = enable_anisotropy_;
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(device_->GetPhysicalDevice()->Get(),
                                &properties);
  sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_info.mipmapMode = mipmap_mode_;
  sampler_info.mipLodBias = 0.f;
  sampler_info.minLod = min_lod;
  sampler_info.maxLod = max_lod;
  sampler_info.pNext = next;

  if (vkCreateSampler(device_->Get(), &sampler_info, nullptr,
                      &sampler_) != VK_SUCCESS)
    LOG_FATAL("Failed to create texure sampler!");
}

void TextureSampler::Destroy() {
  if (sampler_ == VK_NULL_HANDLE) return;
  vkDestroySampler(device_->Get(), sampler_, nullptr);
  sampler_ = VK_NULL_HANDLE;
}

VkSampler TextureSampler::Get() { return sampler_; }

VkFilter TextureSampler::GetMagFilter() const { return mag_filter_; }

VkFilter TextureSampler::GetMinFilter() const { return min_filter_; }

AddressMode TextureSampler::GetAddressMode() const { return address_mode_; }

float TextureSampler::GetAnisotropyEnable() const { return enable_anisotropy_; }

TextureSampler& TextureSampler::SetMagFilter(VkFilter filter) {
  if (sampler_ == VK_NULL_HANDLE) mag_filter_ = filter;
  return *this;
}

TextureSampler& TextureSampler::SetMinFilter(VkFilter filter) {
  if (sampler_ == VK_NULL_HANDLE) min_filter_ = filter;
  return *this;
}

TextureSampler& TextureSampler::SetAddressMode(AddressMode address_mode) {
  if (sampler_ == VK_NULL_HANDLE) address_mode_ = address_mode;
  return *this;
}

TextureSampler& TextureSampler::SetAnisotropyEnable(bool enable) {
  if (sampler_ == VK_NULL_HANDLE) enable_anisotropy_ = enable;
  return *this;
}

TextureSampler& TextureSampler::SetMipmapMode(VkSamplerMipmapMode mode) {
  mipmap_mode_ = mode;

  return *this;
}

TextureSampler& TextureSampler::SetDefaults() {
  SetMagFilter(VK_FILTER_LINEAR)
      .SetMinFilter(VK_FILTER_LINEAR)
      .SetAddressMode({VK_SAMPLER_ADDRESS_MODE_REPEAT,
                       VK_SAMPLER_ADDRESS_MODE_REPEAT,
                       VK_SAMPLER_ADDRESS_MODE_REPEAT})
      .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
      .SetAnisotropyEnable(false);

  return *this;
}

}  // namespace Renderer