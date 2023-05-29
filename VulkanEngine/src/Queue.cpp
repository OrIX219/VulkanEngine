#include "Queue.h"

namespace Renderer {

void Queue::SetQueue(const VkQueue& queue) { queue_ = queue; }

VkQueue Queue::GetQueue() { return queue_; }

void Queue::Submit(VkSubmitInfo submits) {
  submit_batches_.push_back(std::forward<VkSubmitInfo>(submits));
}

VkResult Queue::Present(const VkPresentInfoKHR* present_info) {
  return vkQueuePresentKHR(queue_, present_info);
}

VkResult Queue::SubmitBatches(VkFence fence) {
  VkResult res =
      vkQueueSubmit(queue_, static_cast<uint32_t>(submit_batches_.size()),
                    submit_batches_.data(), fence);
  submit_batches_.clear();
  return res;
}

void Queue::WaitIdle() const { vkQueueWaitIdle(queue_); }

}  // namespace Renderer