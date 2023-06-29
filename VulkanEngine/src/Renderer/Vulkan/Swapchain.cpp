#include "Swapchain.h"

#include <limits>

namespace Renderer {

Swapchain::Swapchain() : swapchain_(VK_NULL_HANDLE) {}

Swapchain::~Swapchain() {}

VkResult Swapchain::Create(LogicalDevice* device, Surface* surface) {
  device_ = device;
  surface_ = surface;

  SwapChainSupportDetails details =
      device_->GetPhysicalDevice()->QuerySwapChainSupport();

  VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(details.formats);
  VkPresentModeKHR present_mode = ChooseSwapPresentMode(details.present_modes);
  VkExtent2D extent = ChooseSwapExtent(details.capabilities);

  uint32_t image_count = details.capabilities.minImageCount + 1;
  if (details.capabilities.maxImageCount > 0 &&
      image_count > details.capabilities.maxImageCount)
    image_count = details.capabilities.maxImageCount;

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface_->Get();
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndicies indicies = device_->GetQueueFamilies();
  uint32_t queue_family_indicies[] = {indicies.graphics_family.value(),
                                      indicies.present_family.value()};

  if (indicies.graphics_family != indicies.present_family) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = queue_family_indicies;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
  }

  create_info.preTransform = details.capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = VK_NULL_HANDLE;

  VkResult res =
      vkCreateSwapchainKHR(device_->Get(), &create_info, nullptr, &swapchain_);
  if (res != VK_SUCCESS) return res;

  vkGetSwapchainImagesKHR(device_->Get(), swapchain_, &image_count, nullptr);
  images_.resize(image_count);
  vkGetSwapchainImagesKHR(device_->Get(), swapchain_, &image_count,
                          images_.data());

  format_ = surface_format.format;
  extent_ = extent;

  return CreateImageViews();
}

void Swapchain::Destroy() {
  for (auto image_view : image_views_)
    vkDestroyImageView(device_->Get(), image_view, nullptr);
  vkDestroySwapchainKHR(device_->Get(), swapchain_, nullptr);
}

VkResult Swapchain::Recreate() {
  Destroy();
  return Create(device_, surface_);
}

VkResult Swapchain::AcquireNextImage(uint32_t* image_index,
                                     VkSemaphore semaphore, VkFence fence,
                                     uint32_t timeout) {
  return vkAcquireNextImageKHR(device_->Get(), swapchain_, timeout, semaphore,
                               fence, image_index);
}

VkSwapchainKHR Swapchain::Get() const { return swapchain_; }

const std::vector<VkImage>& Swapchain::GetImages() const {
  return images_;
}

VkImage Swapchain::GetImage(size_t index) const { return images_.at(index); }

const std::vector<VkImageView>& Swapchain::GetImageViews() const {
  return image_views_;
}

VkImageView Swapchain::GetImageView(size_t index) const {
  return image_views_.at(index);
}

VkFormat Swapchain::GetImageFormat() const { return format_; }

VkExtent2D Swapchain::GetImageExtent() const { return extent_; }

LogicalDevice* Swapchain::GetLogicalDevice() { return device_; }

Surface* Swapchain::GetSurface() { return surface_; }

VkResult Swapchain::CreateImageViews() {
  image_views_.resize(images_.size());

  for (size_t i = 0; i < images_.size(); ++i) {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = images_[i];
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = format_;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    VkResult res = vkCreateImageView(device_->Get(), &create_info, nullptr,
                                     &image_views_[i]);
    if (res != VK_SUCCESS) return res;
  }

  return VK_SUCCESS;
}

VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& available_formats) {
  for (const auto& available_format : available_formats) {
    if (available_format.format == VK_FORMAT_R8G8B8A8_SRGB &&
        available_format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
      return available_format;
  }

  return available_formats[0];
}

VkPresentModeKHR Swapchain::ChooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& available_present_modes) {
  for (const auto& available_present_mode : available_present_modes) {
    if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
      return available_present_mode;
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::ChooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities) {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    return capabilities.currentExtent;

  int width, height;
  surface_->GetWindow()->GetFramebufferSize(&width, &height);

  VkExtent2D actual_extent = {static_cast<uint32_t>(width),
                              static_cast<uint32_t>(height)};

  actual_extent.width =
      std::clamp(actual_extent.width, capabilities.minImageExtent.width,
                 capabilities.maxImageExtent.width);
  actual_extent.height =
      std::clamp(actual_extent.height, capabilities.minImageExtent.height,
                 capabilities.maxImageExtent.height);

  return actual_extent;
}

}  // namespace Renderer