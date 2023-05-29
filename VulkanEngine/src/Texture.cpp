#include "Texture.h"

#include "Buffer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb\stb_image.h>

#include "TextureAsset.h"

namespace Renderer {

Texture::Texture() {}

Texture::Texture(VmaAllocator allocator, LogicalDevice* device,
                 CommandBuffer& command_buffer, const char* path) {
  LoadFromFile(allocator, device, command_buffer, path);
}

bool Texture::LoadFromFile(VmaAllocator allocator, LogicalDevice* device,
                           CommandBuffer command_buffer, const char* path) {
  int tex_width, tex_height, tex_channels;
  stbi_uc* pixels =
      stbi_load(path, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
  VkDeviceSize image_size = tex_width * tex_height * 4;

  if (!pixels) return false;

  staging_buffer_.Destroy();
  staging_buffer_.Create(
      allocator, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  staging_buffer_.SetData(pixels, image_size);

  stbi_image_free(pixels);

  VkExtent3D extent{static_cast<uint32_t>(tex_width),
                    static_cast<uint32_t>(tex_height), 1};
  image_.Create(allocator, device, extent,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

  image_.TransitionLayout(command_buffer, image_.GetFormat(),
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  staging_buffer_.CopyToImage(command_buffer, &image_);

  image_.TransitionLayout(command_buffer, image_.GetFormat(),
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  return true;
}

bool Texture::LoadFromAsset(VmaAllocator allocator, LogicalDevice* device,
                            CommandBuffer command_buffer, const char* path) {
  Assets::AssetFile file;
  bool loaded = Assets::LoadBinaryFile(path, file);

  if (!loaded) {
    std::cerr << "Error when loading texture\n";
    return false;
  }

  Assets::TextureInfo texture_info = Assets::ReadTextureInfo(file);

  VkDeviceSize image_size = texture_info.texture_size;
  VkFormat image_format;
  switch (texture_info.texture_format) {
    case Assets::TextureFormat::RGBA8:
      image_format = VK_FORMAT_R8G8B8A8_SRGB;
      break;
    default:
      return false;
  }

  staging_buffer_.Destroy();
  staging_buffer_.Create(
      allocator, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  
  Assets::UnpackTexture(&texture_info, file.binary_blob.data(),
                        file.binary_blob.size(),
                        static_cast<char*>(staging_buffer_.GetMappedMemory()));

  VkExtent3D extent{static_cast<uint32_t>(texture_info.pixel_size[0]),
                    static_cast<uint32_t>(texture_info.pixel_size[1]), 1};
  image_.Create(allocator, device, extent,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

  image_.TransitionLayout(command_buffer, image_.GetFormat(),
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  staging_buffer_.CopyToImage(command_buffer, &image_);

  image_.TransitionLayout(command_buffer, image_.GetFormat(),
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  return true;
}

void Texture::Destroy() {
  image_.Destroy();
  staging_buffer_.Destroy();
}

VkImage Texture::GetImage() { return image_.GetImage(); }

VkImageView Texture::GetView() { return image_.GetView(); }

}  // namespace Renderer