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

  Renderer::Image::LayoutTransitionInfo layout_info{};
  layout_info.dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
  layout_info.new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  layout_info.src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  layout_info.dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  image_.LayoutTransition(command_buffer, layout_info);

  staging_buffer_.CopyTo(command_buffer, image_);

  layout_info.src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
  layout_info.dst_access = VK_ACCESS_SHADER_READ_BIT;
  layout_info.src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  layout_info.dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  layout_info.new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  layout_info.aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
  image_.GenerateMipMaps(command_buffer, layout_info);

  return true;
}

void Texture::Destroy() {
  image_.Destroy();
  staging_buffer_.Destroy();
}

void Texture::ReleaseStagingMemory() { staging_buffer_.Destroy(); }

VkImage Texture::GetImage() { return image_.Get(); }

VkImageView Texture::GetView() { return image_.GetView(); }

}  // namespace Renderer