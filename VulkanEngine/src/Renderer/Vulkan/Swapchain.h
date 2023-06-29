#pragma once

#include "vulkan/vulkan.hpp"

#include <vector>

#include "LogicalDevice.h"
#include "Surface.h"

namespace Renderer {

class Swapchain {
 public:
  Swapchain();
  ~Swapchain();

  VkResult Create(LogicalDevice* device, Surface* surface);
  void Destroy();

  /*
  Destroy and create swapchain for same surface
  - Used for handling window resize
  - Fetches current window size automatically
  */
  VkResult Recreate();

  VkResult AcquireNextImage(
      uint32_t* image_index, VkSemaphore semaphore,
      VkFence fence = VK_NULL_HANDLE,
      uint32_t timeout = std::numeric_limits<uint64_t>::max());

  VkSwapchainKHR Get() const;
  const std::vector<VkImage>& GetImages() const;
  VkImage GetImage(size_t index) const;
  const std::vector<VkImageView>& GetImageViews() const;
  VkImageView GetImageView(size_t index) const;
  VkFormat GetImageFormat() const;
  VkExtent2D GetImageExtent() const;

  LogicalDevice* GetLogicalDevice();
  Surface* GetSurface();

 private:
  VkResult CreateImageViews();

  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR>& available_formats);
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR>& available_present_modes);
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

  VkSwapchainKHR swapchain_;
  std::vector<VkImage> images_;
  std::vector<VkImageView> image_views_;
  VkFormat format_;
  VkExtent2D extent_;

  LogicalDevice* device_;
  Surface* surface_;
};

}  // namespace Renderer