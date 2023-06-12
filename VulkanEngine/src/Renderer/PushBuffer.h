#pragma once

#include <vma\include\vk_mem_alloc.h>

#include "Buffer.h"

namespace Renderer {

class PushBuffer {
 public:
  PushBuffer();

  void Create(VmaAllocator allocator, uint64_t size, uint32_t alignment);
  void Destroy();

  uint32_t Push(void* data, uint32_t size);

  template<typename T>
  uint32_t Push(T& data) {
    return Push(&data, sizeof(data));
  }

  void Reset();

  VkBuffer GetBuffer();

 private:
  uint32_t PadUniformBufferSize(uint32_t original_size);

  uint32_t current_offset_;
  uint32_t align_;
  Buffer<true> buffer_;
};

}