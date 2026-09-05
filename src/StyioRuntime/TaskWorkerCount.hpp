#pragma once
#ifndef STYIO_RUNTIME_TASK_WORKER_COUNT_HPP_
#define STYIO_RUNTIME_TASK_WORKER_COUNT_HPP_

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace styio::runtime {

/// Single authority for the task scheduler's worker-thread count.
///
/// The scheduler in StyioExtern sizes its worker pool from this value, and an
/// observation session must publish at least this many producer lanes so every
/// worker owns exactly one SPSC lane. Keep both consumers on this helper so the
/// two can never diverge (STYIO_TASK_THREADS override included).
inline std::uint32_t
configured_task_worker_count() {
  std::size_t count = std::thread::hardware_concurrency();
  if (const char* raw = std::getenv("STYIO_TASK_THREADS")) {
    char* end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(raw, &end, 10);
    if (errno == 0 && end != raw && parsed > 0) {
      count = static_cast<std::size_t>(parsed);
    }
  }
  if (count == 0) {
    count = 1;
  }
  return static_cast<std::uint32_t>(std::min<std::size_t>(count, 64));
}

} // namespace styio::runtime

#endif
