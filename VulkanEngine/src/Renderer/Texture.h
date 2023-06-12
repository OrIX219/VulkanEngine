#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>

#include "Buffer.h"
#include "Image.h"

namespace Renderer {

class Texture {
 public:
  Texture();
  Texture(VmaAllocator allocator, LogicalDevice* device,
          CommandBuffer& command_buffer, const char* path);

  bool LoadFromAsset(VmaAllocator allocator, LogicalDevice* device,
                     CommandBuffer command_buffer, const char* path);
  void Destroy();

  VkImage GetImage();
  VkImageView GetView();

 private:
  uint32_t CalculateMipLevels(uint32_t width, uint32_t height);
  void GenerateMipmaps(CommandBuffer command_buffer);

  Buffer<true> staging_buffer_;
  Image image_;
};

}