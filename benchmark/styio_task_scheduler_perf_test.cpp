#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "StyioExtern/ExternLib.hpp"

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kTaskCount = 4;
constexpr auto kSleepPerTask = std::chrono::milliseconds(160);

int64_t
sleep_i64_task(void*) {
  std::this_thread::sleep_for(kSleepPerTask);
  return 1;
}

int64_t
noop_i64_task(void*) {
  return 1;
}

long long
elapsed_ms(const Clock::time_point& start, const Clock::time_point& end) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

long long
elapsed_us(const Clock::time_point& start, const Clock::time_point& end) {
  return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

}  // namespace

TEST(StyioTaskSchedulerPerf, SleepTasksRunConcurrently) {
  setenv("STYIO_TASK_THREADS", "4", 1);
  styio_runtime_clear_error();

  const auto seq_start = Clock::now();
  int64_t sequential_sum = 0;
  for (int i = 0; i < kTaskCount; ++i) {
    sequential_sum += sleep_i64_task(nullptr);
  }
  const auto seq_ms = elapsed_ms(seq_start, Clock::now());

  std::vector<int64_t> handles;
  handles.reserve(kTaskCount);
  const auto parallel_start = Clock::now();
  for (int i = 0; i < kTaskCount; ++i) {
    handles.push_back(styio_task_i64_spawn(&sleep_i64_task, nullptr));
  }

  int64_t parallel_sum = 0;
  for (const int64_t handle : handles) {
    parallel_sum += styio_task_i64_pull(handle);
  }
  const auto parallel_ms = elapsed_ms(parallel_start, Clock::now());

  for (const int64_t handle : handles) {
    styio_task_release(handle);
  }

  EXPECT_EQ(sequential_sum, kTaskCount);
  EXPECT_EQ(parallel_sum, kTaskCount);
  EXPECT_GE(styio_task_worker_count(), 4);
  EXPECT_EQ(styio_task_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);

  std::cout << "styio_task_scheduler_perf sequential_ms=" << seq_ms
            << " parallel_ms=" << parallel_ms
            << " speedup="
            << (parallel_ms > 0 ? static_cast<double>(seq_ms) / static_cast<double>(parallel_ms) : 0.0)
            << " workers=" << styio_task_worker_count() << "\n";

  EXPECT_LT(parallel_ms, seq_ms * 70 / 100)
    << "sequential_ms=" << seq_ms << " parallel_ms=" << parallel_ms;
}

TEST(StyioTaskSchedulerPerf, NoopFanoutOverhead) {
  setenv("STYIO_TASK_THREADS", "4", 1);
  styio_runtime_clear_error();

  int task_count = 10000;
  if (const char* raw = std::getenv("STYIO_TASK_NOOP_COUNT")) {
    char* end = nullptr;
    const long parsed = std::strtol(raw, &end, 10);
    if (end != raw && parsed > 0 && parsed < 1000000) {
      task_count = static_cast<int>(parsed);
    }
  }

  std::vector<int64_t> handles;
  handles.reserve(static_cast<std::size_t>(task_count));
  const auto start = Clock::now();
  for (int i = 0; i < task_count; ++i) {
    handles.push_back(styio_task_i64_spawn(&noop_i64_task, nullptr));
  }

  int64_t sum = 0;
  for (const int64_t handle : handles) {
    sum += styio_task_i64_pull(handle);
  }
  const auto total_us = elapsed_us(start, Clock::now());

  for (const int64_t handle : handles) {
    styio_task_release(handle);
  }

  EXPECT_EQ(sum, task_count);
  EXPECT_GE(styio_task_worker_count(), 4);
  EXPECT_EQ(styio_task_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);

  const double per_task_us = static_cast<double>(total_us) / static_cast<double>(task_count);
  std::cout << "styio_task_scheduler_noop tasks=" << task_count
            << " total_us=" << total_us
            << " per_task_us=" << per_task_us
            << " workers=" << styio_task_worker_count() << "\n";
}
