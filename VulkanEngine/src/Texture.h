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

  bool LoadFromFile(VmaAllocator allocator, LogicalDevice* device,
                    CommandBuffer command_buffer, const char* path);
  bool LoadFromAsset(VmaAllocator allocator, LogicalDevice* device,
                     CommandBuffer command_buffer, const char* path);
  void Destroy();

  VkImage GetImage();
  VkImageView GetView();

 private:
  Buffer<true> staging_buffer_;
  Image image_;
};

}