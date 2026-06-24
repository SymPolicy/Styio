#pragma once
#ifndef STYIO_BENCH_INTERNAL_BENCH_UTILS_HPP_
#define STYIO_BENCH_INTERNAL_BENCH_UTILS_HPP_

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace styio::bench {

struct BenchmarkSample {
  std::string phase;       // "lex", "parse", "sema", "type", "topology", "diag", "runtime"
  std::string label;       // descriptive sub-label
  int64_t duration_ns = 0;
  int64_t alloc_count = 0; // 0 if not measured
};

struct BenchmarkResult {
  std::string schema = "styio.benchmark.v1";
  std::string git_sha;
  std::string build_type;
  int64_t timestamp = 0;
  std::vector<BenchmarkSample> samples;

  std::string to_json() const;
};

// Timer that records elapsed time in nanoseconds.
class Timer {
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  Timer() : start_(Clock::now()) {}

  void reset() { start_ = Clock::now(); }

  // Elapsed in nanoseconds.
  int64_t elapsed_ns() const {
    auto end = Clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
  }

  // Elapsed in microseconds.
  int64_t elapsed_us() const {
    auto end = Clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
  }

private:
  TimePoint start_;
};

// Run a benchmark function multiple times and return a sample with median time.
// `warmup` iterations are run first and discarded.
// `measured` iterations are timed; the median is used for the sample.
template <typename F>
BenchmarkSample run_benchmark(
    std::string phase,
    std::string label,
    int warmup,
    int measured,
    F&& func) {
  // Warmup
  for (int i = 0; i < warmup; ++i) {
    func();
  }

  // Measured iterations
  std::vector<int64_t> times;
  times.reserve(measured);
  for (int i = 0; i < measured; ++i) {
    Timer t;
    func();
    times.push_back(t.elapsed_ns());
  }

  // Median
  std::sort(times.begin(), times.end());
  int64_t median_ns = times[times.size() / 2];

  BenchmarkSample sample;
  sample.phase = std::move(phase);
  sample.label = std::move(label);
  sample.duration_ns = median_ns;
  return sample;
}

// Simple inline JSON serialization (no third-party JSON lib dependency).
inline std::string BenchmarkResult::to_json() const {
  std::string out;
  out.reserve(4096);
  out += "{\n";
  out += "  \"schema\": \"" + schema + "\",\n";
  out += "  \"git_sha\": \"" + git_sha + "\",\n";
  out += "  \"build_type\": \"" + build_type + "\",\n";
  out += "  \"timestamp\": " + std::to_string(timestamp) + ",\n";
  out += "  \"samples\": [\n";
  for (size_t i = 0; i < samples.size(); ++i) {
    const auto& s = samples[i];
    out += "    {";
    out += "\"phase\": \"" + s.phase + "\", ";
    out += "\"label\": \"" + s.label + "\", ";
    out += "\"duration_ns\": " + std::to_string(s.duration_ns);
    if (s.alloc_count > 0) {
      out += ", \"alloc_count\": " + std::to_string(s.alloc_count);
    }
    out += "}";
    if (i + 1 < samples.size()) out += ",";
    out += "\n";
  }
  out += "  ]\n";
  out += "}\n";
  return out;
}

} // namespace styio::bench

#endif // STYIO_BENCH_INTERNAL_BENCH_UTILS_HPP_
