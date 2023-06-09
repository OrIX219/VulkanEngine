#include "TextureCube.h"

#include "Logger.h"
#include "TextureAsset.h"

namespace Renderer {

// Order is important!
const std::array<const char*, 6> TextureCube::faces_ = {
    "right", "left", "top", "bottom", "front", "back"};

TextureCube::TextureCube() {}

TextureCube::TextureCube(VmaAllocator allocator, LogicalDevice* device,
                         CommandBuffer& command_buffer, const char* path) {
  LoadFromDirectory(allocator, device, command_buffer, path);
}

bool TextureCube::LoadFromDirectory(VmaAllocator allocator,
                                    LogicalDevice* device,
                                    CommandBuffer command_buffer,
                                    const char* path) {
  uint64_t full_size = 0;
  uint64_t face_size = 0; // Should be same for all faces
  std::array<char*, 6> face_data;
  Assets::TextureInfo texture_info;
  for (uint32_t i = 0; i < 6; ++i) {
    std::string face_path = std::string{path} + '/' + faces_[i] + ".tx";
    Assets::AssetFile file;
    bool loaded = Assets::LoadBinaryFile(face_path.c_str(), file);

    if (!loaded) {
      LOG_ERROR("Failed to load {} cube face of '{}'", faces_[i], path);
      return false;
    }

    texture_info = Assets::ReadTextureInfo(file);

    face_size = texture_info.texture_size;
    full_size += face_size;

    face_data[i] = static_cast<char*>(malloc(face_size));

    Assets::UnpackTexture(&texture_info, file.binary_blob.data(),
                          file.binary_blob.size(), face_data[i]);
  }

  if (staging_buffer_.GetSize() < full_size) {
    staging_buffer_.Destroy();
    staging_buffer_.Create(
        allocator, full_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  }

  char* buffer_data = staging_buffer_.GetMappedMemory<char>();
  for (uint32_t i = 0; i < 6; ++i) {
    memcpy(buffer_data + (i * face_size), face_data[i], face_size);
    free(face_data[i]);
  }

  VkExtent3D extent{static_cast<uint32_t>(texture_info.pixel_size[0]),
                    static_cast<uint32_t>(texture_info.pixel_size[1]), 1};
  image_.Create(allocator, device, extent,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT);

  Renderer::Image::LayoutTransitionInfo layout_info{};
  layout_info.dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
  layout_info.new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  layout_info.src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  layout_info.dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  image_.LayoutTransition(command_buffer, layout_info);

  staging_buffer_.CopyTo(command_buffer, image_);

  layout_info.src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
  layout_info.dst_access = VK_ACCESS_SHADER_READ_BIT;
  layout_info.new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  layout_info.src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  layout_info.dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  image_.LayoutTransition(command_buffer, layout_info);

  return true;
}

void TextureCube::Destroy() {
  image_.Destroy();
  staging_buffer_.Destroy();
}

void TextureCube::ReleaseStagingMemory() { staging_buffer_.Destroy(); }

VkImage TextureCube::GetImage() { return image_.Get(); }

VkImageView TextureCube::GetView() { return image_.GetView(); }

}  // namespace Renderer