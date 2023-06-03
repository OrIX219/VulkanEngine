#include "VulkanInstance.h"
#include "Logger.h"

namespace Renderer {

VulkanInstance::VulkanInstance() : instance_(VK_NULL_HANDLE) {}

VulkanInstance::~VulkanInstance() {}

VkResult VulkanInstance::Init(bool enable_validation_layers,
                              std::vector<const char*> validation_layers) {
  enable_validation_layers_ = enable_validation_layers;
  validation_layers_ = std::move(validation_layers);
  if (enable_validation_layers_ && !CheckValidationLayerSupport())
    return VK_ERROR_VALIDATION_FAILED_EXT;

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Hello, Triangle!";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

  auto extensions = GetRequiredExtensions();
  create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
  if (enable_validation_layers_) {
    create_info.enabledLayerCount =
        static_cast<uint32_t>(validation_layers_.size());
    create_info.ppEnabledLayerNames = validation_layers_.data();

    debug_create_info.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_create_info.messageSeverity =
        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_create_info.pfnUserCallback = Engine::Logger::DebugCallback;
    debug_create_info.pUserData = nullptr;

    create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
  } else {
    create_info.enabledLayerCount = 0;
    create_info.pNext = nullptr;
  }

  return vkCreateInstance(&create_info, nullptr, &instance_);
}

void VulkanInstance::Destroy() { vkDestroyInstance(instance_, nullptr); }

VkInstance VulkanInstance::GetInstance() { return instance_; }

bool VulkanInstance::ValidationLayersEnabled() const {
  return enable_validation_layers_;
}

const std::vector<const char*>& VulkanInstance::GetValidationLayers() const {
  return validation_layers_;
}

bool VulkanInstance::CheckValidationLayerSupport() {
  uint32_t layer_count;
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

  std::vector<VkLayerProperties> available_layers(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

  for (const char* layer_name : validation_layers_) {
    bool layer_found = false;

    for (const auto& layer_properties : available_layers) {
      if (strcmp(layer_name, layer_properties.layerName) == 0) {
        layer_found = true;
        break;
      }
    }

    if (!layer_found) return false;
  }

  return true;
}

std::vector<const char*> VulkanInstance::GetRequiredExtensions() {
  uint32_t glfw_extensions_count = 0;
  const char** glfw_extensions =
      glfwGetRequiredInstanceExtensions(&glfw_extensions_count);

  std::vector<const char*> extensions(glfw_extensions,
                                      glfw_extensions + glfw_extensions_count);

  if (enable_validation_layers_)
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  return extensions;
}

}  // namespace Renderer