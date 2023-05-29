#include "CommandBuffer.h"
#include "CommandPool.h"

namespace Renderer {
CommandBuffer::CommandBuffer(CommandPool* command_pool, uint32_t pool_index)
    : pool_(command_pool), pool_index_(pool_index), command_buffer_(VK_NULL_HANDLE) {}
CommandBuffer::CommandBuffer(CommandPool* command_pool, uint32_t pool_index,
                             VkCommandBufferLevel level)
    : pool_(command_pool), pool_index_(pool_index) {
  Create(level);
}

CommandBuffer::~CommandBuffer() {}

VkCommandBuffer CommandBuffer::GetBuffer() { return command_buffer_; }

VkCommandBuffer* CommandBuffer::GetBufferPtr() { return &command_buffer_; }

VkResult CommandBuffer::Reset() {
  return vkResetCommandBuffer(command_buffer_, 0);
}

VkResult CommandBuffer::Begin(bool one_time) {
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  if (one_time)
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  return vkBeginCommandBuffer(command_buffer_, &begin_info);
}

VkResult CommandBuffer::End() {
  return vkEndCommandBuffer(command_buffer_);
}

void CommandBuffer::Submit() {
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers =
      &pool_->command_buffers_[pool_index_].command_buffer_;

  Queue& queue = pool_->GetLogicalDevice()->GetQueue(pool_->GetQueueFamily());
  queue.Submit(submit_info);
}

void CommandBuffer::Submit(VkSubmitInfo submit_info) {
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer_;
  Queue& queue = pool_->GetLogicalDevice()->GetQueue(pool_->GetQueueFamily());
  queue.Submit(std::move(submit_info));
}

VkResult CommandBuffer::Create(VkCommandBufferLevel level) {
  VkCommandBufferAllocateInfo alloc_info =
      CommandBuffer::AllocateInfo(pool_->GetPool(), 1, level);

  return vkAllocateCommandBuffers(pool_->GetLogicalDevice()->GetDevice(),
                                  &alloc_info, &command_buffer_);
}

void CommandBuffer::Destroy() {
  if (command_buffer_ == VK_NULL_HANDLE) return;

  vkFreeCommandBuffers(pool_->GetLogicalDevice()->GetDevice(), pool_->GetPool(),
                       1, &command_buffer_);
}

VkCommandBufferAllocateInfo CommandBuffer::AllocateInfo(
    VkCommandPool command_pool, uint32_t count, VkCommandBufferLevel level) {
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = command_pool;
  alloc_info.level = level;
  alloc_info.commandBufferCount = count;
  return alloc_info;
}

}  // namespace Renderer