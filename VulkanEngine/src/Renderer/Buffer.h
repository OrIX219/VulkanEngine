#pragma once

#include <vulkan/vulkan.hpp>
#include <vma\include\vk_mem_alloc.h>

#include "CommandPool.h"
#include "LogicalDevice.h"
#include "Image.h"

namespace Renderer {

class BufferBase {
 public:
  BufferBase() : buffer_(VK_NULL_HANDLE), buffer_size_(0) {}

  VkBuffer GetBuffer() { return buffer_; }
  VkDeviceSize GetSize() const { return buffer_size_; }

  VkDescriptorBufferInfo GetDescriptorInfo() const {
    VkDescriptorBufferInfo info{};
    info.buffer = buffer_;
    info.range = buffer_size_;
    return info;
  }

 protected:
  VkBuffer buffer_;
  VkDeviceSize buffer_size_;
  VmaAllocation allocation_;
};

template<bool persistently_mapped = false>
class Buffer : public BufferBase {
 public:
  Buffer() {}

  Buffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
         VmaAllocationCreateFlags alloc_flags = 0) {
    Create(allocator, size, usage, alloc_flags);
  }
  
  VkResult Create(VmaAllocator allocator, VkDeviceSize size,
              VkBufferUsageFlags usage,
              VmaAllocationCreateFlags alloc_flags = 0) {
    allocator_ = allocator;
    buffer_size_ = size;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size_;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = alloc_flags;

    VkResult res = vmaCreateBuffer(allocator_, &buffer_info, &alloc_info,
                                   &buffer_, &allocation_, nullptr);
    if (res != VK_SUCCESS) return res;

    if constexpr (persistently_mapped)
      res = vmaMapMemory(allocator_, allocation_, &mapped_memory_);
    return res;
  }

  void Destroy() {
    if (buffer_ == VK_NULL_HANDLE) return;

    if constexpr (persistently_mapped) vmaUnmapMemory(allocator_, allocation_);

    vmaDestroyBuffer(allocator_, buffer_, allocation_);

    buffer_ = VK_NULL_HANDLE;
  }

  template<typename T = void>
  T* GetMappedMemory() {
    return reinterpret_cast<T*>(mapped_memory_);
  }

  void SetData(const void* data, size_t size, VkDeviceSize offset = 0) {
    char* addr = static_cast<char*>(mapped_memory_) + offset;
    if constexpr (persistently_mapped) {
      memcpy(addr, data, size);
    } else {
      vmaMapMemory(allocator_, allocation_, &mapped_memory_);
      memcpy(addr, data, size);
      vmaUnmapMemory(allocator_, allocation_);
    }
  }

  void CopyTo(CommandBuffer command_buffer, BufferBase& dst,
              VkDeviceSize offset = 0) const {
    VkBufferCopy copy_region{};
    copy_region.size = buffer_size_;
    copy_region.dstOffset = offset;
    vkCmdCopyBuffer(command_buffer.GetBuffer(), buffer_,
                    dst.GetBuffer(), 1, &copy_region);
  }

  void CopyToImage(CommandBuffer command_buffer, Image& image) {
    VkBufferImageCopy copy_region{};
    copy_region.bufferOffset = 0;
    copy_region.bufferRowLength = 0;
    copy_region.bufferImageHeight = 0;
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.mipLevel = 0;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageOffset = {0, 0, 0};
    copy_region.imageExtent = image.GetExtent();

    vkCmdCopyBufferToImage(
        command_buffer.GetBuffer(), buffer_, image.GetImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
  }

 private:
  VmaAllocator allocator_;

  void* mapped_memory_;
};

}