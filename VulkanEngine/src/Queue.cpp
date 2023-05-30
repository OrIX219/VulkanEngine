#include "Queue.h"

namespace Renderer {

Queue::Queue() : current_batch_(0) {}

void Queue::SetQueue(const VkQueue& queue) { queue_ = queue; }

VkQueue Queue::GetQueue() { return queue_; }

void Queue::StartBatch() {
  batches_.push_back(std::vector<VkCommandBuffer>());
}

void Queue::AddToBatch(VkCommandBuffer command_buffer) {
  if (current_batch_ >= batches_.size()) StartBatch();
  batches_[current_batch_].push_back(command_buffer);
}

void Queue::EndBatch() {
  if (batches_[current_batch_].size() > 0) {
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount =
        static_cast<uint32_t>(batches_[current_batch_].size());
    submit_info.pCommandBuffers = batches_[current_batch_].data();
    Submit(std::move(submit_info));
    ++current_batch_;
  }
}

void Queue::EndBatch(VkSubmitInfo sync_info) {
  if (batches_[current_batch_].size() > 0) {
    sync_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sync_info.commandBufferCount =
        static_cast<uint32_t>(batches_[current_batch_].size());
    sync_info.pCommandBuffers = batches_[current_batch_].data();
    Submit(std::move(sync_info));
    ++current_batch_;
  }
}

void Queue::Submit(VkSubmitInfo submits) {
  submit_batches_.push_back(std::forward<VkSubmitInfo>(submits));
}

VkResult Queue::Present(const VkPresentInfoKHR* present_info) {
  return vkQueuePresentKHR(queue_, present_info);
}

VkResult Queue::SubmitBatches(VkFence fence) {
  if (current_batch_ < batches_.size()) EndBatch();

  VkResult res =
      vkQueueSubmit(queue_, static_cast<uint32_t>(submit_batches_.size()),
                    submit_batches_.data(), fence);
  submit_batches_.clear();
  batches_.clear();
  current_batch_ = 0;
  return res;
}

void Queue::WaitIdle() const { vkQueueWaitIdle(queue_); }

}  // namespace Renderer