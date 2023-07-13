#include "LogicalDevice.h"

#include <unordered_set>

namespace Renderer {

LogicalDevice::LogicalDevice() : device_(VK_NULL_HANDLE) {}

LogicalDevice::~LogicalDevice() {}

VkResult LogicalDevice::Init(PhysicalDevice* physical_device) {
  physical_device_ = physical_device;
  queue_family_indices_ = physical_device_->FindQueueFamilies();

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  std::unordered_set<uint32_t> unique_queue_families{
      queue_family_indices_.graphics_family.value(),
      queue_family_indices_.present_family.value(),
      queue_family_indices_.transfer_family.value()};

  float queue_priority = 1.f;
  for (uint32_t queue_family : unique_queue_families) {
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(queue_create_info);
  }

  VkPhysicalDeviceFeatures features{};
  features.samplerAnisotropy = VK_TRUE;
  features.sampleRateShading = VK_TRUE;
  features.pipelineStatisticsQuery = VK_TRUE;
  features.geometryShader = VK_TRUE;
  features.multiDrawIndirect = VK_TRUE;
  features.fillModeNonSolid = VK_TRUE;
  features.imageCubeArray = VK_TRUE;
  features.independentBlend = VK_TRUE;
  VkPhysicalDeviceFeatures2 device_features{};
  device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  device_features.features = features;
  VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_params{};
  shader_draw_params.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
  shader_draw_params.shaderDrawParameters = VK_TRUE;
  device_features.pNext = &shader_draw_params;
  VkPhysicalDeviceVulkan12Features device_features2{};
  device_features2.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  device_features2.samplerFilterMinmax = VK_TRUE;
  device_features2.drawIndirectCount = VK_TRUE;
  device_features2.pNext = &device_features;

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pNext = &device_features2;
  create_info.queueCreateInfoCount =
      static_cast<uint32_t>(queue_create_infos.size());
  create_info.pQueueCreateInfos = queue_create_infos.data();
  const auto& extensions = physical_device_->GetExtensions();
  create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();
  if (physical_device_->GetInstance()->ValidationLayersEnabled()) {
    const auto& layers = physical_device_->GetInstance()->GetValidationLayers();
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();
  } else {
    create_info.enabledLayerCount = 0;
  }

  VkResult res = vkCreateDevice(physical_device_->Get(), &create_info,
                                nullptr, &device_);
  if (res != VK_SUCCESS) return res;

  VkQueue graphics_queue, present_queue, transfer_queue;
  vkGetDeviceQueue(device_, queue_family_indices_.graphics_family.value(), 0,
                   &graphics_queue);
  vkGetDeviceQueue(device_, queue_family_indices_.present_family.value(), 0,
                   &present_queue);
  vkGetDeviceQueue(device_, queue_family_indices_.transfer_family.value(), 0,
                   &transfer_queue);
  queues_.resize(queue_family_indices_.MaxIndex() + 1);
  queues_[queue_family_indices_.graphics_family.value()].Set(
      graphics_queue);
  queues_[queue_family_indices_.present_family.value()].Set(present_queue);
  queues_[queue_family_indices_.transfer_family.value()].Set(
      transfer_queue);

  return res;
}

void LogicalDevice::Destroy() { vkDestroyDevice(device_, nullptr); }

VkDevice LogicalDevice::Get() { return device_; }

PhysicalDevice* LogicalDevice::GetPhysicalDevice() { return physical_device_; }

QueueFamilyIndicies LogicalDevice::GetQueueFamilies() const {
  return queue_family_indices_;
}

Queue& LogicalDevice::GetQueue(uint32_t family_index) { return queues_[family_index]; }

Queue& LogicalDevice::GetGraphicsQueue() {
  return queues_[queue_family_indices_.graphics_family.value()];
}

Queue& LogicalDevice::GetPresentQueue() {
  return queues_[queue_family_indices_.present_family.value()];
}

Queue& LogicalDevice::GetTransferQueue() {
  return queues_[queue_family_indices_.transfer_family.value()];
}

void LogicalDevice::WaitIdle() const { vkDeviceWaitIdle(device_); }

}  // namespace Renderer