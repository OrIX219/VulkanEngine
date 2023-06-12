#pragma once

#include <vulkan/vulkan.hpp>
#include <vma\include\vk_mem_alloc.h>

#include "Vulkan/CommandPool.h"
#include "Vulkan/LogicalDevice.h"

namespace Renderer {

class Image {
 public:
  Image();
  Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
        VkImageUsageFlags usage, uint32_t mip_levels = 1,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
        VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT);

  VkResult Create(VmaAllocator allocator, LogicalDevice* device,
                  VkExtent3D extent, VkImageUsageFlags usage,
                  uint32_t mip_levels = 1,
                  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                  VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
                  VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
                  VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT);
  void Destroy();

  VkImage GetImage();
  VkImageView GetView();

  VkExtent3D GetExtent() const;
  VkFormat GetFormat() const;
  uint32_t GetMipLevels() const;

  void TransitionLayout(CommandBuffer command_buffer, VkFormat format,
                        VkImageLayout old_layout, VkImageLayout new_layout);

  static VkFormat FindSupportedFormat(PhysicalDevice* physical_device,
                                      const std::vector<VkFormat>& candidates,
                                      VkImageTiling tiling,
                                      VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties(physical_device->GetDevice(), format,
                                          &props);

      if (tiling == VK_IMAGE_TILING_LINEAR &&
          (props.linearTilingFeatures & features) == features)
        return format;
      else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features)
        return format;
    }
    return VK_FORMAT_UNDEFINED;
  }

 protected:
  VkImage image_;
  VmaAllocation allocation_;

  VkImageView image_view_;

  VkExtent3D image_extent_;
  VkFormat image_format_;
  uint32_t mip_levels_;

  VmaAllocator allocator_;
  LogicalDevice* device_;

 private:
  VkResult CreateImageView(
      VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT);
};

}