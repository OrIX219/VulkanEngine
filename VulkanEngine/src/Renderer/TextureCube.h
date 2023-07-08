#pragma once

#include <vulkan/vulkan.hpp>
#include <vma\include\vk_mem_alloc.h>

#include "Buffer.h"
#include "Image.h"

namespace Renderer {

class TextureCube {
 public:
  TextureCube();
  TextureCube(VmaAllocator allocator, LogicalDevice* device,
              CommandBuffer& command_buffer, const char* path);

  bool LoadFromDirectory(VmaAllocator allocator, LogicalDevice* device,
                         CommandBuffer command_buffer, const char* path);
  void Destroy();

  void ReleaseStagingMemory();

  VkImage GetImage();
  VkImageView GetView();

 private:
  static const std::array<const char*, 6> faces_;

  Buffer<true> staging_buffer_;
  ImageCube image_;
};

}