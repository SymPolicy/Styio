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
  int64_t duration_ns = 0; // median
  int64_t mean_ns = 0;
  int64_t min_ns = 0;
  int64_t max_ns = 0;
  int64_t p95_ns = 0;
  int warmup = 0;
  int iterations = 0;
  int input_size = 0;
  int64_t alloc_count = 0; // 0 if not measured
};

struct BenchmarkResult {
  std::string schema = "styio.benchmark.v2";
  std::string git_sha;
  std::string build_type;
  std::string compiler = "clang";
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
// `measured` iterations are timed; median, mean, p95, min, max are computed.
template <typename F>
BenchmarkSample run_benchmark(
    std::string phase,
    std::string label,
    int warmup,
    int measured,
    F&& func,
    int input_size = 0) {
  // Warmup
  for (int i = 0; i < warmup; ++i) {
    func();
  }

  // Measured iterations
  std::vector<int64_t> times;
  times.reserve(static_cast<std::size_t>(measured));
  for (int i = 0; i < measured; ++i) {
    Timer t;
    func();
    times.push_back(t.elapsed_ns());
  }

  std::sort(times.begin(), times.end());

  BenchmarkSample sample;
  sample.phase = std::move(phase);
  sample.label = std::move(label);
  sample.warmup = warmup;
  sample.iterations = measured;
  sample.input_size = input_size;
  sample.duration_ns = times[times.size() / 2];  // median

  // Compute mean
  int64_t sum = 0;
  for (auto t : times) sum += t;
  sample.mean_ns = sum / static_cast<int64_t>(times.size());

  sample.min_ns = times.front();
  sample.max_ns = times.back();

  // p95
  std::size_t p95_idx = static_cast<std::size_t>(times.size() * 0.95);
  if (p95_idx >= times.size()) p95_idx = times.size() - 1;
  sample.p95_ns = times[p95_idx];

  return sample;
}

// Simple inline JSON serialization (no third-party JSON lib dependency).
inline std::string BenchmarkResult::to_json() const {
  std::string out;
  out.reserve(8192);
  out += "{\n";
  out += "  \"schema\": \"" + schema + "\",\n";
  out += "  \"commit\": \"" + git_sha + "\",\n";
  out += "  \"build_type\": \"" + build_type + "\",\n";
  out += "  \"compiler\": \"" + compiler + "\",\n";
  out += "  \"timestamp\": " + std::to_string(timestamp) + ",\n";
  out += "  \"benchmarks\": [\n";
  for (size_t i = 0; i < samples.size(); ++i) {
    const auto& s = samples[i];
    out += "    {\n";
    out += "      \"name\": \"" + s.phase + "/" + s.label + "\",\n";
    out += "      \"phase\": \"" + s.phase + "\",\n";
    out += "      \"label\": \"" + s.label + "\",\n";
    out += "      \"warmup\": " + std::to_string(s.warmup) + ",\n";
    out += "      \"iterations\": " + std::to_string(s.iterations) + ",\n";
    out += "      \"input_size\": " + std::to_string(s.input_size) + ",\n";
    out += "      \"median_ns\": " + std::to_string(s.duration_ns) + ",\n";
    out += "      \"mean_ns\": " + std::to_string(s.mean_ns) + ",\n";
    out += "      \"p95_ns\": " + std::to_string(s.p95_ns) + ",\n";
    out += "      \"min_ns\": " + std::to_string(s.min_ns) + ",\n";
    out += "      \"max_ns\": " + std::to_string(s.max_ns);
    if (s.alloc_count > 0) {
      out += ",\n      \"alloc_count\": " + std::to_string(s.alloc_count);
    }
    out += "\n    }";
    if (i + 1 < samples.size()) out += ",";
    out += "\n";
  }
  out += "  ]\n";
  out += "}\n";
  return out;
}

} // namespace styio::bench

#endif // STYIO_BENCH_INTERNAL_BENCH_UTILS_HPP_
