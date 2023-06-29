#pragma once

#include "vulkan/vulkan.hpp"

#include <vector>

namespace Renderer {

class Queue {
 public:
  Queue();

  void Set(const VkQueue& queue);
  VkQueue Get();

  /*
  Begin new command buffer batch

  - Ends current batch if there is any
  */
  void BeginBatch();
  /*
  Add command buffer to current command buffer batch

  - Begins new batch if there is none
  */
  void AddToBatch(VkCommandBuffer command_buffer);
  /*
  End current command buffer batch and push it to buffer for later submission
  */
  void EndBatch();
  void EndBatch(VkSubmitInfo sync_info);

  /*
  Push submit info to buffer for later submission
  */
  void Submit(VkSubmitInfo submits);

  /*
  Present swapchain image
  */
  VkResult Present(const VkPresentInfoKHR* present_info);

  /*
  Submit all buffered command buffer batches to queue for execution
  */
  VkResult SubmitBatches(VkFence fence = VK_NULL_HANDLE);

  /*
  Wait for all submitted to queue command buffer batches to finish execution
  */
  void WaitIdle() const;

 private:
  VkQueue queue_;

  uint32_t current_batch_;
  std::vector<std::vector<VkCommandBuffer>> batches_;
  std::vector<VkSubmitInfo> submit_batches_;
};

}