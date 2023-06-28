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
  if (view_type_ == VK_IMAGE_VIEW_TYPE_CUBE)
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
  vkDestroyImageView(device_->GetDevice(), image_view_, nullptr);
  vmaDestroyImage(allocator_, image_, allocation_);
}

VkImage Image::GetImage() { return image_; }

VkImageView Image::GetView() { return image_view_; }

VkExtent3D Image::GetExtent() const { return image_extent_; }

VkFormat Image::GetFormat() const { return image_format_; }

uint32_t Image::GetMipLevels() const { return mip_levels_; }

uint32_t Image::GetArrayLayers() const { return array_layers_; }

VkImageViewType Image::GetViewType() const { return view_type_; }

VkImageLayout Image::GetLayout() const { return current_layout_; }

void Image::TransitionLayout(CommandBuffer command_buffer,
                             VkAccessFlags src_access, VkAccessFlags dst_access,
                             VkImageLayout new_layout,
                             VkPipelineStageFlags src_stage,
                             VkPipelineStageFlags dst_stage,
                             VkImageAspectFlags aspect_flags,
                             VkDependencyFlags dependency) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = src_access;
  barrier.dstAccessMask = dst_access;
  barrier.oldLayout = current_layout_;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image_;
  barrier.subresourceRange.aspectMask = aspect_flags;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  vkCmdPipelineBarrier(command_buffer.GetBuffer(), src_stage, dst_stage,
                       dependency, 0, nullptr, 0, nullptr, 1, &barrier);
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

  return vkCreateImageView(device_->GetDevice(), &create_info, nullptr,
                           &image_view_);
}

}  // namespace Renderer