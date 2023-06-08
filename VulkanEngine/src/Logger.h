#pragma once

#include "vulkan/vulkan.hpp"

#include <chrono>
#include <iostream>
#include <string_view>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/color.h>
#include "VulkanInstance.h"

#define LOG_FATAL(message, ...) \
  Engine::Logger::Get().Log(Engine::LogType::kFatal, message, ##__VA_ARGS__)
#define LOG_ERROR(message, ...) \
  Engine::Logger::Get().Log(Engine::LogType::kError, message, ##__VA_ARGS__)
#define LOG_WARNING(message, ...) \
  Engine::Logger::Get().Log(Engine::LogType::kWarning, message, ##__VA_ARGS__)
#define LOG_INFO(message, ...) \
  Engine::Logger::Get().Log(Engine::LogType::kInfo, message, ##__VA_ARGS__)
#define LOG_SUCCESS(message, ...) \
  Engine::Logger::Get().Log(Engine::LogType::kSuccess, message, ##__VA_ARGS__)

namespace Engine {

enum class LogType { kFatal, kError, kInfo, kWarning, kSuccess };

class Logger {
 public:
  static void Init(Renderer::VulkanInstance* instance) {
    Get().instance_ = instance;
    if (!Get().instance_->ValidationLayersEnabled()) return;

    VkDebugUtilsMessengerCreateInfoEXT create_info;
    create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = DebugCallback;
    create_info.pUserData = nullptr;

    CreateDebugUtilsMessengerEXT(Get().instance_->GetInstance(), &create_info,
                                 nullptr, &Get().debug_messenger_);
  }

  static void Cleanup() {
    if (!Get().instance_->ValidationLayersEnabled()) return;
    DestroyDebugUtilsMessengerEXT(Get().instance_->GetInstance(),
                                  Get().debug_messenger_, nullptr);
  }

  template<typename...Args>
  inline static void Print(std::string_view message, Args... args) {
    fmt::print((message), args...);
    fmt::print("\n");
  }

  template <typename... Args>
  inline static void Log(LogType type, std::string_view message, Args... args) {
    PrintTime();
    switch (type) {
      case Engine::LogType::kFatal:
        fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "[FATAL]   ");
        break;
      case Engine::LogType::kError:
        fmt::print(fg(fmt::color::crimson), "[ERROR]   ");
        break;
      case Engine::LogType::kWarning:
        fmt::print(fg(fmt::color::yellow), "[WARNING] ");
        break;
      case Engine::LogType::kInfo:
        fmt::print(fg(fmt::color::white), "[INFO]    ");
        break;
      case Engine::LogType::kSuccess:
        fmt::print(fg(fmt::color::light_green), "[SUCCESS] ");
        break;
    }
    Print(message, args...);

    if (type == LogType::kFatal) abort();
  }

  inline static void PrintTime() {
    std::chrono::time_point<std::chrono::system_clock> now =
        std::chrono::system_clock::now();
    auto time_point = now - Logger::Get().start_time;
    fmt::print("[{:%M:%S}]", time_point);
  }

  inline static Logger& Get() {
    static Logger logger{};
    return logger;
  }

  void SetTime() { start_time = std::chrono::system_clock::now(); }

  std::chrono::time_point<std::chrono::system_clock> start_time;

  static VKAPI_ATTR VkBool32 VKAPI_CALL
  DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void* pUserData) {
    switch (messageSeverity) {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        Get().Log(LogType::kError, pCallbackData->pMessage);
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        Get().Log(LogType::kWarning, pCallbackData->pMessage);
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        Get().Log(LogType::kInfo, pCallbackData->pMessage);
        break;
    }

    return VK_FALSE;
  }

 private:
  static VkResult CreateDebugUtilsMessengerEXT(
     VkInstance instance,
     const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
     const VkAllocationCallbacks* pAllocator,
     VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
      return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
  }

  static void DestroyDebugUtilsMessengerEXT(
      VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
      const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
      func(instance, debugMessenger, pAllocator);
    }
  }

  VkDebugUtilsMessengerEXT debug_messenger_;

  Renderer::VulkanInstance* instance_;
};

}  // namespace Renderer