#include "PhysicalDevice.h"

#include <unordered_set>

namespace Renderer {

PhysicalDevice::PhysicalDevice() : device_(VK_NULL_HANDLE) {}

PhysicalDevice::~PhysicalDevice() {}

VkResult PhysicalDevice::Init(VulkanInstance* instance, Surface* surface,
                          std::vector<const char*> device_extensions) {
  instance_ = instance;
  surface_ = surface;
  extensions_ = std::move(device_extensions);

  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(instance_->GetInstance(), &device_count, nullptr);
  if (device_count == 0) return VK_ERROR_DEVICE_LOST;

  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(instance_->GetInstance(), &device_count,
                             devices.data());

  for (const auto& device : devices) {
    if (IsDeviceSuitable(device)) {
      device_ = device;
      break;
    }
  }

  if (device_ == VK_NULL_HANDLE) return VK_ERROR_DEVICE_LOST;

  vkGetPhysicalDeviceProperties(device_, &properties_);

  VkSampleCountFlags counts = properties_.limits.framebufferColorSampleCounts &
                              properties_.limits.framebufferDepthSampleCounts;
  max_samples_ = static_cast<VkSampleCountFlagBits>((counts + 1) >> 1);

  return VK_SUCCESS;
}

VkPhysicalDevice PhysicalDevice::GetDevice() { return device_; }

VkPhysicalDeviceProperties PhysicalDevice::GetProperties() const {
  return properties_;
}

VkSampleCountFlagBits PhysicalDevice::GetMaxSamples() const {
  return max_samples_;
}

const std::vector<const char*>& PhysicalDevice::GetExtensions() const {
  return extensions_;
}

const VulkanInstance* PhysicalDevice::GetInstance() const { return instance_; }

const Surface* PhysicalDevice::GetSurface() const { return surface_; }

QueueFamilyIndicies PhysicalDevice::FindQueueFamilies() {
  return FindQueueFamilies(device_);
}

SwapChainSupportDetails PhysicalDevice::QuerySwapChainSupport() {
  return QuerySwapChainSupport(device_);
}

QueueFamilyIndicies PhysicalDevice::FindQueueFamilies(VkPhysicalDevice device) {
  QueueFamilyIndicies indicies;

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           nullptr);

  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           queue_families.data());

  int i = 0;
  for (const auto& family : queue_families) {
    if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      indicies.graphics_family = i;
    else if (family.queueFlags & VK_QUEUE_TRANSFER_BIT)
      indicies.transfer_family = i;
    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_->GetSurface(),
                                         &present_support);
    if (present_support && !indicies.present_family.has_value())
      indicies.present_family = i;

    if (indicies.IsComplete()) break;

    ++i;
  }

  return indicies;
}

SwapChainSupportDetails PhysicalDevice::QuerySwapChainSupport(
    VkPhysicalDevice device) {
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_->GetSurface(),
                                            &details.capabilities);

  uint32_t format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_->GetSurface(),
                                       &format_count, nullptr);
  if (format_count != 0) {
    details.formats.resize(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_->GetSurface(),
                                         &format_count, details.formats.data());
  }

  uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_->GetSurface(),
                                            &present_mode_count, nullptr);
  if (present_mode_count != 0) {
    details.present_modes.resize(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_->GetSurface(),
                                              &present_mode_count,
                                              details.present_modes.data());
  }

  return details;
}

bool PhysicalDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensions_count = 0;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensions_count,
                                       nullptr);

  std::vector<VkExtensionProperties> available_extensions(extensions_count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensions_count,
                                       available_extensions.data());

  std::unordered_set<std::string> required_extensions(extensions_.begin(),
                                                      extensions_.end());

  for (const auto& extension : available_extensions)
    required_extensions.erase(extension.extensionName);

  return required_extensions.empty();
}

bool PhysicalDevice::IsDeviceSuitable(VkPhysicalDevice device) {
  QueueFamilyIndicies indicies = FindQueueFamilies(device);

  bool extensions_supported = CheckDeviceExtensionSupport(device);
  bool swap_chain_adequate = false;
  if (extensions_supported) {
    SwapChainSupportDetails details = QuerySwapChainSupport(device);
    swap_chain_adequate = !details.formats.empty() && !details.present_modes.empty();
  }

  VkPhysicalDeviceFeatures supported_features;
  vkGetPhysicalDeviceFeatures(device, &supported_features);

  bool features_support = supported_features.samplerAnisotropy &&
                          supported_features.sampleRateShading &&
                          supported_features.pipelineStatisticsQuery;

  return indicies.IsComplete() && extensions_supported && swap_chain_adequate &&
         features_support;
}

}  // namespace Renderer