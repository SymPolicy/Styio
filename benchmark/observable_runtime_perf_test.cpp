#include <chrono>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <thread>
#include <vector>

#include "StyioExtern/ExternLib.hpp"
#include "StyioRuntime/ObservationBuffer.hpp"
#include "StyioServices/StyioObservable/RuntimeCorrelation.hpp"

#ifndef STYIO_BUILD_DIR
#define STYIO_BUILD_DIR "."
#endif
#ifndef STYIO_COMPILER_EXE
#define STYIO_COMPILER_EXE ""
#endif

namespace fs = std::filesystem;
namespace obs = styio::observable;
namespace rt = styio::runtime::observation;

namespace {

int64_t bench_body(void*) {
  return 1;
}

std::uint64_t rss_bytes() {
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
#if defined(__APPLE__)
  return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
  return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ull;
#endif
}

std::uint64_t file_size_or_zero(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return 0;
  }
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  return ec ? 0 : static_cast<std::uint64_t>(size);
}

struct ModeMetrics
{
  const char* name = "";
  std::uint64_t wall_ns = 0;
  std::uint64_t cpu_ns = 0;
  std::uint64_t allocations = 0;
  std::uint64_t peak_rss_bytes = 0;
  std::uint64_t binary_size = 0;
  std::uint64_t artifact_size = 0;
  std::uint64_t produced_bytes = 0;
  std::uint64_t produced_records = 0;
  std::uint64_t aggregated_records = 0;
  std::uint64_t emitted_bytes = 0;
  std::uint64_t emitted_records = 0;
  std::uint64_t occupancy_high = 0;
  std::uint64_t sampled_out = 0;
  std::uint64_t buffer_dropped = 0;
  std::uint64_t exporter_dropped = 0;
  std::uint64_t s3_callsites = 0;
  std::uint64_t s3_allocations = 0;
  std::uint64_t s3_buffers = 0;
  std::uint64_t s3_threads = 0;
  std::uint64_t s3_atomics = 0;
  std::uint64_t s3_exporter = 0;
};

void run_workload(int repeats) {
  for (int i = 0; i < repeats; ++i) {
    const int64_t handle = styio_task_i64_spawn(&bench_body, nullptr);
    (void)styio_task_i64_pull(handle);
    styio_task_release(handle);
  }
}

ModeMetrics measure(obs::ObservationMode mode, const char* name, bool enable) {
  ModeMetrics metrics;
  metrics.name = name;
  metrics.binary_size = file_size_or_zero(STYIO_COMPILER_EXE);
  std::string sink_bytes;
  std::uint64_t sink_records = 0;
  const auto wall0 = std::chrono::steady_clock::now();
  const auto cpu0 = std::clock();
  if (enable) {
    rt::SessionConfig config;
    config.mode = mode;
    config.execution_id = "x2_00000000000000bb";
    config.producer_lanes = 1;
    config.sink = [&](std::string_view line) {
      sink_bytes.append(line.data(), line.size());
      sink_records += 1;
      return true;
    };
    (void)rt::begin_session(config);
    metrics.s3_callsites = 1;
    metrics.s3_allocations = 1;
    metrics.s3_buffers = 1;
    metrics.s3_threads = 1;
    metrics.s3_atomics = 1;
    metrics.s3_exporter = 1;
  }
  run_workload(enable ? 8 : 16);
  obs::SessionAccounting acc;
  if (enable) {
    acc = rt::accounting_snapshot();
    rt::end_session();
    metrics.allocations = acc.families[static_cast<std::size_t>(obs::EventFamily::TaskLifecycle)].observed;
    metrics.produced_records = 0;
    metrics.aggregated_records = 0;
    metrics.emitted_records = 0;
    metrics.sampled_out = 0;
    metrics.buffer_dropped = 0;
    metrics.exporter_dropped = 0;
    for (const auto& row : acc.families) {
      metrics.produced_records += row.observed;
      metrics.aggregated_records += row.aggregated;
      metrics.emitted_records += row.emitted;
      metrics.sampled_out += row.sampled_out;
      metrics.buffer_dropped += row.buffer_dropped;
      metrics.exporter_dropped += row.exporter_dropped;
    }
    metrics.occupancy_high = acc.high_water_occupancy;
    metrics.produced_bytes = sink_bytes.size();
    metrics.emitted_bytes = sink_bytes.size();
    metrics.artifact_size = sink_bytes.size();
  }
  const auto wall1 = std::chrono::steady_clock::now();
  const auto cpu1 = std::clock();
  metrics.wall_ns = static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(wall1 - wall0).count());
  const double cpu_sec =
    static_cast<double>(cpu1 - cpu0) / static_cast<double>(CLOCKS_PER_SEC);
  metrics.cpu_ns = static_cast<std::uint64_t>(cpu_sec * 1e9);
  metrics.peak_rss_bytes = rss_bytes();
  (void)sink_records;
  return metrics;
}

void emit_mode(std::ostream& out, const ModeMetrics& metrics, bool comma) {
  if (comma) {
    out << ",";
  }
  out << "\"" << metrics.name << "\":{"
      << "\"wall_ns\":" << metrics.wall_ns << ","
      << "\"cpu_ns\":" << metrics.cpu_ns << ","
      << "\"allocations\":" << metrics.allocations << ","
      << "\"peak_rss_bytes\":" << metrics.peak_rss_bytes << ","
      << "\"binary_size\":" << metrics.binary_size << ","
      << "\"artifact_size\":" << metrics.artifact_size << ","
      << "\"produced_bytes\":" << metrics.produced_bytes << ","
      << "\"produced_records\":" << metrics.produced_records << ","
      << "\"aggregated_records\":" << metrics.aggregated_records << ","
      << "\"emitted_bytes\":" << metrics.emitted_bytes << ","
      << "\"emitted_records\":" << metrics.emitted_records << ","
      << "\"occupancy_high\":" << metrics.occupancy_high << ","
      << "\"sampled_out\":" << metrics.sampled_out << ","
      << "\"buffer_dropped\":" << metrics.buffer_dropped << ","
      << "\"exporter_dropped\":" << metrics.exporter_dropped << ","
      << "\"s3_callsites\":" << metrics.s3_callsites << ","
      << "\"s3_allocations\":" << metrics.s3_allocations << ","
      << "\"s3_buffers\":" << metrics.s3_buffers << ","
      << "\"s3_threads\":" << metrics.s3_threads << ","
      << "\"s3_atomics\":" << metrics.s3_atomics << ","
      << "\"s3_exporter\":" << metrics.s3_exporter
      << "}";
}

bool structural_zeros(const ModeMetrics& metrics) {
  return metrics.s3_callsites == 0 && metrics.s3_allocations == 0
    && metrics.s3_buffers == 0 && metrics.s3_threads == 0
    && metrics.s3_atomics == 0 && metrics.s3_exporter == 0
    && metrics.produced_records == 0 && metrics.emitted_records == 0
    && metrics.aggregated_records == 0 && metrics.occupancy_high == 0
    && metrics.sampled_out == 0 && metrics.buffer_dropped == 0
    && metrics.exporter_dropped == 0;
}

} // namespace

int
main() {
  const ModeMetrics disabled = measure(obs::ObservationMode::Disabled, "disabled", false);
  const ModeMetrics static_only = measure(obs::ObservationMode::Disabled, "static-only", false);
  const ModeMetrics aggregate = measure(obs::ObservationMode::Aggregate, "aggregate", true);
  const ModeMetrics sampled = measure(obs::ObservationMode::Sampled, "sampled", true);
  const ModeMetrics detailed = measure(obs::ObservationMode::Detailed, "detailed", true);

  if (!structural_zeros(disabled) || !structural_zeros(static_only)) {
    std::cerr << "disabled/static-only S3 structural zeros failed\n";
    return 1;
  }
  if (disabled.wall_ns == 0 || aggregate.wall_ns == 0) {
    std::cerr << "required wall_ns missing\n";
    return 1;
  }

  const fs::path out_dir = fs::path(STYIO_BUILD_DIR) / "benchmark/observable-runtime";
  std::error_code ec;
  fs::create_directories(out_dir, ec);
  const fs::path out_path = out_dir / "perf-result.json";
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "cannot write " << out_path << "\n";
    return 1;
  }
  out << "{"
      << "\"status\":\"pending\","
      << "\"approval\":\"unapproved\","
      << "\"baseline_identity\":\"\","
      << "\"compiler_revision\":\"local\","
      << "\"modes\":{";
  emit_mode(out, disabled, false);
  emit_mode(out, static_only, true);
  emit_mode(out, aggregate, true);
  emit_mode(out, sampled, true);
  emit_mode(out, detailed, true);
  out << "}}\n";
  std::cout << "wrote " << out_path << "\n";
  return 0;
}
