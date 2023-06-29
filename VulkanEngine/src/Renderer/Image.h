#pragma once

#include <vulkan/vulkan.hpp>
#include <vma\include\vk_mem_alloc.h>

#include "CommandPool.h"
#include "LogicalDevice.h"

namespace Renderer {

class Image {
 public:
  Image();
  Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
        VkImageUsageFlags usage, uint32_t mip_levels = 1,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
        VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
        uint32_t array_layers = 1,
        VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D);
  Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
        VkImageUsageFlags usage, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
        VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
  Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
        VkImageUsageFlags usage,
        VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D,
        uint32_t array_layers = 1, uint32_t mip_levels = 1);
  Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
        VkImageUsageFlags usage,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

  VkResult Create(VmaAllocator allocator, LogicalDevice* device,
                  VkExtent3D extent, VkImageUsageFlags usage,
                  uint32_t mip_levels = 1,
                  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                  VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
                  VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
                  VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
                  uint32_t array_layers = 1,
                  VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D);
  VkResult Create(VmaAllocator allocator, LogicalDevice* device,
                  VkExtent3D extent, VkImageUsageFlags usage,
                  VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
                  VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
                  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
  VkResult Create(VmaAllocator allocator, LogicalDevice* device,
                  VkExtent3D extent, VkImageUsageFlags usage,
                  VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D,
                  uint32_t array_layers = 1, uint32_t mip_levels = 1);
  VkResult Create(VmaAllocator allocator, LogicalDevice* device,
                  VkExtent3D extent, VkImageUsageFlags usage,
                  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

  void Destroy();

  VkImage Get();
  VkImageView GetView();

  VkExtent3D GetExtent() const;
  VkFormat GetFormat() const;
  uint32_t GetMipLevels() const;
  uint32_t GetArrayLayers() const;
  VkImageViewType GetViewType() const;
  VkImageLayout GetLayout() const;

  static uint32_t CalculateMipLevels(uint32_t width, uint32_t height) {
    return static_cast<uint32_t>(
               std::floor(std::log2(std::max(width, height)))) +
           1;
  }

  void TransitionLayout(
      CommandBuffer command_buffer, VkAccessFlags src_access,
      VkAccessFlags dst_access, VkImageLayout new_layout,
      VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
      VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
      VkDependencyFlags dependency = 0);

  static VkFormat FindSupportedFormat(PhysicalDevice* physical_device,
                                      const std::vector<VkFormat>& candidates,
                                      VkImageTiling tiling,
                                      VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties(physical_device->Get(), format,
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
  VkImageViewType view_type_;

  VkExtent3D image_extent_;
  VkFormat image_format_;
  uint32_t mip_levels_;
  uint32_t array_layers_;
  VkImageLayout current_layout_;

  VmaAllocator allocator_;
  LogicalDevice* device_;

 private:
  VkResult CreateImageView(
      VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT);
};

}