#pragma once

#include <vulkan/vulkan.hpp>
#include <vma\include\vk_mem_alloc.h>

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
  void GenerateMipmaps(CommandBuffer command_buffer);

  Buffer<true> staging_buffer_;
  Image image_;
};

}