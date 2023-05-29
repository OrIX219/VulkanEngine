#include "CommandPool.h"

namespace Renderer {

CommandPool::CommandPool()
    : command_pool_(VK_NULL_HANDLE),
      available_index_(0) {}

CommandPool::~CommandPool() {}

VkResult CommandPool::Create(LogicalDevice* device, uint32_t queue_family_index,
                             VkCommandPoolCreateFlags flags) {
  device_ = device;
  queue_family_ = queue_family_index;
  flags_ = flags;

  VkCommandPoolCreateInfo create_info =
      CommandPool::CreateInfo(queue_family_, flags_);

  return vkCreateCommandPool(device_->GetDevice(), &create_info, nullptr,
                             &command_pool_);
}

VkResult CommandPool::Create(LogicalDevice* device,
                             VkCommandPoolCreateInfo* create_info) {
  device_ = device;
  queue_family_ = create_info->queueFamilyIndex;
  flags_ = create_info->flags;

  return vkCreateCommandPool(device_->GetDevice(), create_info, nullptr,
                             &command_pool_);
}

void CommandPool::Destroy() {
  if (command_pool_ == VK_NULL_HANDLE) return;
  vkDestroyCommandPool(device_->GetDevice(), command_pool_, nullptr);
}

VkCommandPool CommandPool::GetPool() const { return command_pool_; }

size_t CommandPool::GetBuffersCount() const { return command_buffers_.size(); }

CommandBuffer CommandPool::GetBuffer() {
  if (available_index_ >= command_buffers_.size()) CreateBuffer();
  return command_buffers_[available_index_++];
}

void CommandPool::CreateBuffer(VkCommandBufferLevel level) {
  command_buffers_.push_back(
      {this, static_cast<uint32_t>(command_buffers_.size())});
  command_buffers_.back().Create(level);
}

void CommandPool::Clear() {
  for (size_t i = 0; i < command_buffers_.size(); ++i)
    command_buffers_[i].Destroy();
  command_buffers_.clear();
}

VkResult CommandPool::Reset() {
  available_index_ = 0;
  return vkResetCommandPool(device_->GetDevice(), command_pool_, 0);
}

uint32_t CommandPool::GetQueueFamily() const { return queue_family_; }

LogicalDevice* CommandPool::GetLogicalDevice() { return device_; }

VkCommandPoolCreateInfo CommandPool::CreateInfo(
    uint32_t queue_family, VkCommandPoolCreateFlags flags) {
  VkCommandPoolCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  create_info.flags = flags;
  create_info.queueFamilyIndex = queue_family;
  return create_info;
}
  
}  // namespace Renderer