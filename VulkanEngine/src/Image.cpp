#include "Image.h"

namespace Renderer {

Image::Image() : image_(VK_NULL_HANDLE) {}

Image::Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
             VkImageUsageFlags usage, uint32_t mip_levels, VkFormat format,
             VkImageTiling tiling, VkImageAspectFlags aspect_flags) {
  Create(allocator, device, extent, usage, mip_levels, format, tiling,
         aspect_flags);
}

VkResult Image::Create(VmaAllocator allocator, LogicalDevice* device,
                       VkExtent3D extent, VkImageUsageFlags usage,
                       uint32_t mip_levels, VkFormat format,
                       VkImageTiling tiling, VkImageAspectFlags aspect_flags) {
  allocator_ = allocator;
  device_ = device;
  image_extent_ = extent;
  mip_levels_ = mip_levels;
  image_format_ = format;

  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent = image_extent_;
  image_info.mipLevels = mip_levels_;
  image_info.arrayLayers = 1;
  image_info.format = format;
  image_info.tiling = tiling;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = usage;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkResult res = vmaCreateImage(allocator_, &image_info, &alloc_info, &image_,
                                &allocation_, nullptr);
  if (res != VK_SUCCESS) return res;

  return CreateImageView(aspect_flags);
}

void Image::Destroy() {
  vkDestroyImageView(device_->GetDevice(), image_view_, nullptr);
  vmaDestroyImage(allocator_, image_, allocation_);
}

VkImage Image::GetImage() { return image_; }

VkImageView Image::GetView() { return image_view_; }

VkExtent3D Image::GetExtent() const { return image_extent_; }

VkFormat Image::GetFormat() const { return image_format_; }

uint32_t Image::GetMipLevels() const { return mip_levels_; }

void Image::TransitionLayout(CommandBuffer command_buffer, VkFormat format,
                             VkImageLayout old_layout, VkImageLayout new_layout) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image_;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mip_levels_;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags source_stage;
  VkPipelineStageFlags destination_stage;

  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    return;
  }

  vkCmdPipelineBarrier(command_buffer.GetBuffer(), source_stage,
                       destination_stage, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
}

VkResult Image::CreateImageView(VkImageAspectFlags aspect_flags) {
  VkImageViewCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  create_info.image = image_;
  create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  create_info.format = image_format_;
  create_info.subresourceRange.aspectMask = aspect_flags;
  create_info.subresourceRange.baseMipLevel = 0;
  create_info.subresourceRange.levelCount = mip_levels_;
  create_info.subresourceRange.baseArrayLayer = 0;
  create_info.subresourceRange.layerCount = 1;

  return vkCreateImageView(device_->GetDevice(), &create_info, nullptr,
                           &image_view_);
}

}  // namespace Renderer