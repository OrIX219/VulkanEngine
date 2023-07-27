#include "Image.h"

namespace Renderer {

Image::Image() : image_(VK_NULL_HANDLE), view_type_(VK_IMAGE_VIEW_TYPE_2D) {}

Image::Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
             VkImageUsageFlags usage, uint32_t mip_levels,
             VkSampleCountFlagBits samples, VkFormat format,
             VkImageTiling tiling, VkImageAspectFlags aspect_flags,
             uint32_t array_layers, VkImageViewType view_type) {
  Create(allocator, device, extent, usage, mip_levels, samples, format, tiling,
         aspect_flags, array_layers, view_type);
}

Image::Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
             VkImageUsageFlags usage, VkFormat format,
             VkImageAspectFlags aspect_flags, VkImageViewType view_type,
             uint32_t array_layers, VkSampleCountFlagBits samples) {
  Create(allocator, device, extent, usage, format, aspect_flags, view_type,
         array_layers, samples);
}

Image::Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
             VkImageUsageFlags usage, VkFormat format,
             VkImageAspectFlags aspect_flags, VkSampleCountFlagBits samples) {
  Create(allocator, device, extent, usage, format, aspect_flags, samples);
}

Image::Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
             VkImageUsageFlags usage, VkImageViewType view_type,
             uint32_t array_layers, uint32_t mip_levels) {
  Create(allocator, device, extent, usage, view_type, array_layers, mip_levels);
}

Image::Image(VmaAllocator allocator, LogicalDevice* device, VkExtent3D extent,
             VkImageUsageFlags usage, VkSampleCountFlagBits samples) {
  Create(allocator, device, extent, usage, samples);
}

VkResult Image::Create(VmaAllocator allocator, LogicalDevice* device,
                       VkExtent3D extent, VkImageUsageFlags usage,
                       uint32_t mip_levels, VkSampleCountFlagBits samples,
                       VkFormat format, VkImageTiling tiling,
                       VkImageAspectFlags aspect_flags, uint32_t array_layers,
                       VkImageViewType view_type) {
  allocator_ = allocator;
  device_ = device;
  image_extent_ = extent;
  mip_levels_ = mip_levels;
  array_layers_ = array_layers;
  image_format_ = format;
  view_type_ = view_type;
  current_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent = image_extent_;
  image_info.mipLevels = mip_levels_;
  image_info.arrayLayers = array_layers_;
  image_info.format = image_format_;
  image_info.tiling = tiling;
  image_info.initialLayout = current_layout_;
  image_info.usage = usage;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.samples = samples;
  if (view_type_ == VK_IMAGE_VIEW_TYPE_CUBE ||
      view_type_ == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
    image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkResult res = vmaCreateImage(allocator_, &image_info, &alloc_info, &image_,
                                &allocation_, nullptr);
  if (res != VK_SUCCESS) return res;

  return CreateImageView(aspect_flags);
}

VkResult Image::Create(VmaAllocator allocator, LogicalDevice* device,
                       VkExtent3D extent, VkImageUsageFlags usage,
                       VkFormat format, VkImageAspectFlags aspect_flags,
                       VkImageViewType view_type, uint32_t array_layers,
                       VkSampleCountFlagBits samples) {
  return Create(allocator, device, extent, usage, 1, samples, format,
                VK_IMAGE_TILING_OPTIMAL, aspect_flags, array_layers, view_type);
}

VkResult Image::Create(VmaAllocator allocator, LogicalDevice* device,
                       VkExtent3D extent, VkImageUsageFlags usage,
                       VkFormat format, VkImageAspectFlags aspect_flags,
                       VkSampleCountFlagBits samples) {
  return Create(allocator, device, extent, usage, 1, samples, format,
                VK_IMAGE_TILING_OPTIMAL, aspect_flags);
}

VkResult Image::Create(VmaAllocator allocator, LogicalDevice* device,
                       VkExtent3D extent, VkImageUsageFlags usage,
                       VkImageViewType view_type, uint32_t array_layers,
                       uint32_t mip_levels) {
  return Create(allocator, device, extent, usage, mip_levels,
                VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                array_layers, view_type);
}

VkResult Image::Create(VmaAllocator allocator, LogicalDevice* device,
                       VkExtent3D extent, VkImageUsageFlags usage,
                       VkSampleCountFlagBits samples) {
  return Create(allocator, device, extent, usage, 1, samples);
}

void Image::Destroy() {
  vkDestroyImageView(device_->Get(), image_view_, nullptr);
  vmaDestroyImage(allocator_, image_, allocation_);
}

VkImage Image::Get() { return image_; }

VkImageView Image::GetView() { return image_view_; }

VkExtent3D Image::GetExtent() const { return image_extent_; }

VkFormat Image::GetFormat() const { return image_format_; }

uint32_t Image::GetMipLevels() const { return mip_levels_; }

uint32_t Image::GetArrayLayers() const { return array_layers_; }

VkImageViewType Image::GetViewType() const { return view_type_; }

VkImageLayout Image::GetLayout() const { return current_layout_; }

void Image::LayoutTransition(CommandBuffer command_buffer,
                             const LayoutTransitionInfo& transition_info) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = transition_info.src_access;
  barrier.dstAccessMask = transition_info.dst_access;
  barrier.oldLayout = current_layout_;
  barrier.newLayout = transition_info.new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image_;
  barrier.subresourceRange.aspectMask = transition_info.aspect_flags;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  vkCmdPipelineBarrier(command_buffer.Get(), transition_info.src_stage,
                       transition_info.dst_stage, transition_info.dependency, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

void Image::GenerateMipMaps(CommandBuffer command_buffer,
                            const LayoutTransitionInfo& transition_info,
                            VkFilter filter) {
  Renderer::Image::LayoutTransitionInfo layout_info{};
  layout_info.dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
  layout_info.new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  layout_info.src_stage = transition_info.src_stage;
  layout_info.dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  LayoutTransition(command_buffer, layout_info);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = image_;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = transition_info.aspect_flags;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = array_layers_;

  int32_t mip_width = image_extent_.width;
  int32_t mip_height = image_extent_.height;

  for (uint32_t i = 1; i < mip_levels_; ++i) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = transition_info.src_access;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer.Get(), transition_info.src_stage,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mip_width, mip_height, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mip_width > 1 ? mip_width / 2 : 1,
                          mip_height > 1 ? mip_height / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(command_buffer.Get(), image_,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = transition_info.new_layout;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = transition_info.dst_access;

    vkCmdPipelineBarrier(command_buffer.Get(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                         transition_info.dst_stage, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

    if (mip_width > 1) mip_width /= 2;
    if (mip_height > 1) mip_height /= 2;
  }

  barrier.subresourceRange.baseMipLevel = mip_levels_ - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = transition_info.new_layout;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = transition_info.dst_access;

  vkCmdPipelineBarrier(command_buffer.Get(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                       transition_info.dst_stage, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
}

VkResult Image::CreateImageView(VkImageAspectFlags aspect_flags) {
  VkImageViewCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  create_info.image = image_;
  create_info.viewType = view_type_;
  create_info.format = image_format_;
  create_info.subresourceRange.aspectMask = aspect_flags;
  create_info.subresourceRange.baseMipLevel = 0;
  create_info.subresourceRange.levelCount = mip_levels_;
  create_info.subresourceRange.baseArrayLayer = 0;
  create_info.subresourceRange.layerCount = array_layers_;

  return vkCreateImageView(device_->Get(), &create_info, nullptr,
                           &image_view_);
}

ImageCube::ImageCube() : Image() { view_type_ = VK_IMAGE_VIEW_TYPE_CUBE; }

ImageCube::ImageCube(VmaAllocator allocator, LogicalDevice* device,
                     VkExtent3D extent, VkImageUsageFlags usage,
                     VkFormat format, VkImageAspectFlagBits aspect_flags,
                     uint32_t array_size) {
  Create(allocator, device, extent, usage, format, aspect_flags, array_size);
}

VkResult ImageCube::Create(VmaAllocator allocator, LogicalDevice* device,
                           VkExtent3D extent, VkImageUsageFlags usage,
                           VkFormat format, VkImageAspectFlagBits aspect_flags,
                           uint32_t array_size) {
  VkImageViewType view_type =
      array_size > 1 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
  return Image::Create(allocator, device, extent, usage, 1,
                       VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL,
                       aspect_flags, 6 * array_size, view_type);
}

}  // namespace Renderer