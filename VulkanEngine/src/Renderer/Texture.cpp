#include "Texture.h"

#include "Buffer.h"
#include "TextureAsset.h"
#include "Logger.h"

namespace Renderer {

Texture::Texture() {}

Texture::Texture(VmaAllocator allocator, LogicalDevice* device,
                 CommandBuffer& command_buffer, const char* path) {
  LoadFromAsset(allocator, device, command_buffer, path);
}

bool Texture::LoadFromAsset(VmaAllocator allocator, LogicalDevice* device,
                            CommandBuffer command_buffer, const char* path) {
  Assets::AssetFile file;
  bool loaded = Assets::LoadBinaryFile(path, file);

  if (!loaded) {
    LOG_ERROR("Error when loading texture");
    return false;
  }

  Assets::TextureInfo texture_info = Assets::ReadTextureInfo(file);

  VkDeviceSize image_size = texture_info.texture_size;

  if (staging_buffer_.GetSize() < image_size) {
    staging_buffer_.Destroy();
    staging_buffer_.Create(
        allocator, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  }
  
  Assets::UnpackTexture(&texture_info, file.binary_blob.data(),
                        file.binary_blob.size(),
                        staging_buffer_.GetMappedMemory<char>());

  VkExtent3D extent{static_cast<uint32_t>(texture_info.pixel_size[0]),
                    static_cast<uint32_t>(texture_info.pixel_size[1]), 1};
  uint32_t mip_levels = Image::CalculateMipLevels(extent.width, extent.height);
  image_.Create(allocator, device, extent,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                mip_levels);

  image_.TransitionLayout(command_buffer, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT);

  staging_buffer_.CopyTo(command_buffer, image_);

  GenerateMipmaps(command_buffer);

  return true;
}

void Texture::Destroy() {
  image_.Destroy();
  staging_buffer_.Destroy();
}

void Texture::ReleaseStagingMemory() { staging_buffer_.Destroy(); }

VkImage Texture::GetImage() { return image_.Get(); }

VkImageView Texture::GetView() { return image_.GetView(); }

void Texture::GenerateMipmaps(CommandBuffer command_buffer) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = image_.Get();
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = image_.GetArrayLayers();

  VkExtent3D extent = image_.GetExtent();
  int32_t mip_width = extent.width;
  int32_t mip_height = extent.height;

  for (uint32_t i = 1; i < image_.GetMipLevels(); ++i) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer.Get(), VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

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

    vkCmdBlitImage(command_buffer.Get(), image_.Get(),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_.Get(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer.Get(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

    if (mip_width > 1) mip_width /= 2;
    if (mip_height > 1) mip_height /= 2;
  }

  barrier.subresourceRange.baseMipLevel = image_.GetMipLevels() - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(command_buffer.Get(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}
}  // namespace Renderer