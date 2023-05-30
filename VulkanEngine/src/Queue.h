#pragma once

#include "vulkan/vulkan.hpp"

#include <vector>

namespace Renderer {

class Queue {
 public:
  Queue();

  void SetQueue(const VkQueue& queue);
  VkQueue GetQueue();

  void StartBatch();
  void AddToBatch(VkCommandBuffer command_buffer);
  void EndBatch();
  void EndBatch(VkSubmitInfo sync_info);

  void Submit(VkSubmitInfo submits);

  VkResult Present(const VkPresentInfoKHR* present_info);

  VkResult SubmitBatches(VkFence fence = VK_NULL_HANDLE);

  void WaitIdle() const;

 private:
  VkQueue queue_;

  VkFence last_fence_;

  uint32_t current_batch_;
  std::vector<std::vector<VkCommandBuffer>> batches_;
  std::vector<VkSubmitInfo> submit_batches_;
};

}