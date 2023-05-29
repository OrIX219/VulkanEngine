#pragma once

#include "vulkan/vulkan.hpp"

#include <vector>

namespace Renderer {

class Queue {
 public:
  void SetQueue(const VkQueue& queue);
  VkQueue GetQueue();

  void Submit(VkSubmitInfo submits);

  VkResult Present(const VkPresentInfoKHR* present_info);

  VkResult SubmitBatches(VkFence fence = VK_NULL_HANDLE);

  void WaitIdle() const;

 private:
  VkQueue queue_;

  VkFence last_fence_;

  std::vector<VkSubmitInfo> submit_batches_;
};

}