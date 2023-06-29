#pragma once

#include "vulkan/vulkan.hpp"

#include "PhysicalDevice.h"
#include "Queue.h"

namespace Renderer {

class LogicalDevice {
 public:
  LogicalDevice();
  ~LogicalDevice();

  VkResult Init(PhysicalDevice* physical_device);
  void Destroy();

  VkDevice Get();
  PhysicalDevice* GetPhysicalDevice();

  QueueFamilyIndicies GetQueueFamilies() const;

  Queue& GetQueue(uint32_t family_index);
  Queue& GetGraphicsQueue();
  Queue& GetPresentQueue();
  Queue& GetTransferQueue();

  void WaitIdle() const;

 private:
  VkDevice device_;
  PhysicalDevice* physical_device_;

  QueueFamilyIndicies queue_family_indices_;
  std::vector<Queue> queues_;
};

}  // namespace Renderer