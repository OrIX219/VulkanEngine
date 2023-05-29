#pragma once

#include "vulkan/vulkan.hpp"

#include <optional>

#include "VulkanInstance.h"
#include "Surface.h"

namespace Renderer {

struct QueueFamilyIndicies {
  std::optional<uint32_t> graphics_family;
  std::optional<uint32_t> transfer_family;
  std::optional<uint32_t> present_family;

  bool IsComplete() const {
    return graphics_family.has_value() && present_family.has_value() &&
           transfer_family.has_value();
  }

  uint32_t MaxIndex() const {
    return std::max({graphics_family.value(), present_family.value(),
                     transfer_family.value()});
  }
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

class PhysicalDevice {
 public:
  PhysicalDevice();
  ~PhysicalDevice();

  VkResult Init(VulkanInstance* instance, Surface* surface,
                std::vector<const char*> device_extensions = {});

  VkPhysicalDevice GetDevice();
  VkPhysicalDeviceProperties GetProperties() const;

  const std::vector<const char*>& GetExtensions() const;
  const VulkanInstance* GetInstance() const;
  const Surface* GetSurface() const;

  QueueFamilyIndicies FindQueueFamilies();
  SwapChainSupportDetails QuerySwapChainSupport();

 private:
  QueueFamilyIndicies FindQueueFamilies(VkPhysicalDevice device);
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

  bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

  bool IsDeviceSuitable(VkPhysicalDevice device);

  VkPhysicalDevice device_ = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties properties_;

  VulkanInstance* instance_;
  Surface* surface_;

  std::vector<const char*> extensions_;
};

}  // namespace Renderer