#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "CommandBuffer.h"
#include "LogicalDevice.h"

namespace Renderer {

class VulkanProfiler;

struct ScopeTimer {
  uint32_t start_timestamp;
  uint32_t end_timestamp;
  std::string name;
};

struct StatRecorder {
  uint32_t query;
  std::string name;
};

class VulkanScopeTimer {
 public:
  VulkanScopeTimer(CommandBuffer command_buffer, VulkanProfiler* profiler,
                   const char* name);
  ~VulkanScopeTimer();

 private:
  VulkanProfiler* profiler_;
  CommandBuffer command_buffer_;
  ScopeTimer timer_;
};

class VulkanPipelineStatRecorder {
 public:
  VulkanPipelineStatRecorder(CommandBuffer command_buffer,
                             VulkanProfiler* profiler, const char* name);
  ~VulkanPipelineStatRecorder();

 private:
  VulkanProfiler* profiler_;
  CommandBuffer command_buffer_;
  StatRecorder stat_recorder_;
};

class VulkanProfiler {
 public:
  void Init(LogicalDevice* device, float timestamp_period,
            uint32_t per_frame_pool_sizes = 100);
  void Destroy();

  void GrabQueries(CommandBuffer command_buffer);

  double GetStat(const std::string& name);

  VkQueryPool GetTimerPool();
  VkQueryPool GetStatPool();

  void AddTimer(ScopeTimer& timer);
  void AddStat(StatRecorder& stat);

  uint32_t GetTimestampId();
  uint32_t GetStatId();

  std::unordered_map<std::string, double> timings;
  std::unordered_map<std::string, int32_t> stats;

 private:
  struct QueryStatFrame {
    std::vector<ScopeTimer> timers;
    VkQueryPool timer_pool;
    uint32_t timer_last;

    std::vector<StatRecorder> stat_recorders;
    VkQueryPool stat_pool;
    uint32_t stat_last;
  };

  static constexpr uint32_t kMaxFramesInFlight = 2;
  
  uint32_t current_frame_;
  float period_;
  std::array<QueryStatFrame, kMaxFramesInFlight> query_frames_;

  LogicalDevice* device_;
};

}