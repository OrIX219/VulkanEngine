#include "VulkanProfiler.h"

namespace Renderer {

VulkanScopeTimer::VulkanScopeTimer(CommandBuffer command_buffer,
                                   VulkanProfiler* profiler, const char* name)
    : profiler_(profiler), command_buffer_(command_buffer) {
  timer_.name = name;
  timer_.start_timestamp = profiler_->GetTimestampId();

  VkQueryPool pool = profiler_->GetTimerPool();

  vkCmdWriteTimestamp(command_buffer_.GetBuffer(),
                      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool,
                      timer_.start_timestamp);
}

VulkanScopeTimer::~VulkanScopeTimer() {
  timer_.end_timestamp = profiler_->GetTimestampId();

  VkQueryPool pool = profiler_->GetTimerPool();

  vkCmdWriteTimestamp(command_buffer_.GetBuffer(),
                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pool,
                      timer_.end_timestamp);

  profiler_->AddTimer(timer_);
}

VulkanPipelineStatRecorder::VulkanPipelineStatRecorder(
    CommandBuffer command_buffer, VulkanProfiler* profiler, const char* name)
    : profiler_(profiler), command_buffer_(command_buffer) {
  stat_recorder_.name = name;
  stat_recorder_.query = profiler_->GetStatId();

  vkCmdBeginQuery(command_buffer_.GetBuffer(), profiler_->GetStatPool(),
                  stat_recorder_.query, 0);
}

VulkanPipelineStatRecorder::~VulkanPipelineStatRecorder() {
  vkCmdEndQuery(command_buffer_.GetBuffer(), profiler_->GetStatPool(),
                stat_recorder_.query);

  profiler_->AddStat(stat_recorder_);
}

void VulkanProfiler::Init(LogicalDevice* device, float timestamp_period,
                          uint32_t per_frame_pool_sizes) {
  period_ = timestamp_period;
  device_ = device;
  current_frame_ = 0;
  uint32_t pool_size = per_frame_pool_sizes;

  VkQueryPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  pool_info.queryCount = pool_size;

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    vkCreateQueryPool(device_->GetDevice(), &pool_info, nullptr,
                      &query_frames_[i].timer_pool);
    query_frames_[i].timer_last = per_frame_pool_sizes;
  }

  pool_info.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    pool_info.pipelineStatistics =
        VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;
    vkCreateQueryPool(device_->GetDevice(), &pool_info, nullptr,
                      &query_frames_[i].stat_pool);
    query_frames_[i].stat_last = per_frame_pool_sizes;
  }
}

void VulkanProfiler::Cleanup() {
  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    vkDestroyQueryPool(device_->GetDevice(), query_frames_[i].timer_pool,
                       nullptr);
    vkDestroyQueryPool(device_->GetDevice(), query_frames_[i].stat_pool,
                       nullptr);
  }
}

void VulkanProfiler::GrabQueries(CommandBuffer command_buffer) {
  uint32_t frame = current_frame_;
  current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;

  vkCmdResetQueryPool(command_buffer.GetBuffer(),
                      query_frames_[current_frame_].timer_pool, 0,
                      query_frames_[current_frame_].timer_last);
  query_frames_[current_frame_].timer_last = 0;
  query_frames_[current_frame_].timers.clear();
  vkCmdResetQueryPool(command_buffer.GetBuffer(),
                      query_frames_[current_frame_].stat_pool, 0,
                      query_frames_[current_frame_].stat_last);
  query_frames_[current_frame_].stat_last = 0;
  query_frames_[current_frame_].stat_recorders.clear();

  QueryStatFrame& state = query_frames_[frame];
  std::vector<uint64_t> query_state;
  query_state.resize(state.timer_last);
  if (state.timer_last != 0) {
    vkGetQueryPoolResults(device_->GetDevice(), state.timer_pool, 0,
                          state.timer_last,
                          query_state.size() * sizeof(query_state[0]),
                          query_state.data(), sizeof(query_state[0]),
                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  }
  std::vector<uint64_t> stat_results;
  stat_results.resize(state.stat_last);
  if (state.stat_last != 0) {
    vkGetQueryPoolResults(device_->GetDevice(), state.stat_pool, 0,
                          state.stat_last,
                          stat_results.size() * sizeof(stat_results[0]),
                          stat_results.data(), sizeof(stat_results[0]),
                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  }

  for (ScopeTimer& timer : state.timers) {
    uint64_t begin = query_state[timer.start_timestamp];
    uint64_t end = query_state[timer.end_timestamp];
    uint64_t timestamp = end - begin;
    timings[timer.name] =
        (static_cast<double>(timestamp) * period_) / 1000000.0; 
  }
  for (StatRecorder& stat_recorder : state.stat_recorders) {
    uint64_t result = stat_results[stat_recorder.query];
    stats[stat_recorder.name] = static_cast<int32_t>(result);
  }
}

double VulkanProfiler::GetStat(const std::string& name) {
  auto iter = timings.find(name);
  if (iter != timings.end()) return iter->second;
  return 0;
}

VkQueryPool VulkanProfiler::GetTimerPool() {
  return query_frames_[current_frame_].timer_pool;
}

VkQueryPool VulkanProfiler::GetStatPool() {
  return query_frames_[current_frame_].stat_pool;
}

void VulkanProfiler::AddTimer(ScopeTimer& timer) {
  query_frames_[current_frame_].timers.push_back(timer);
}

void VulkanProfiler::AddStat(StatRecorder& stat) {
  query_frames_[current_frame_].stat_recorders.push_back(stat);
}

uint32_t VulkanProfiler::GetTimestampId() {
  uint32_t id = query_frames_[current_frame_].timer_last;
  ++query_frames_[current_frame_].timer_last;
  return id;
}

uint32_t VulkanProfiler::GetStatId() {
  uint32_t id = query_frames_[current_frame_].stat_last;
  ++query_frames_[current_frame_].stat_last;
  return id;
}

}  // namespace Renderer