#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "StyioToken/Token.hpp"

namespace styio::profiler {

class FrontendProfiler
{
public:
  class PhaseScope
  {
  public:
    PhaseScope() = default;
    PhaseScope(FrontendProfiler* profiler, const char* name);
    PhaseScope(const PhaseScope&) = delete;
    PhaseScope& operator=(const PhaseScope&) = delete;
    PhaseScope(PhaseScope&& other) noexcept;
    PhaseScope& operator=(PhaseScope&& other) noexcept;
    ~PhaseScope();

  private:
    FrontendProfiler* profiler_ = nullptr;
    std::string name_;
    std::chrono::steady_clock::time_point started_at_;
    bool active_ = false;
  };

  void enable(std::string file_path, std::string parser_engine, std::string output_path);
  bool enabled() const noexcept;
  bool written() const noexcept;
  const std::string& output_path() const noexcept;

  PhaseScope phase(const char* name);
  void record_phase(std::string name, std::int64_t duration_ns);

  void set_source_summary(std::uint64_t source_bytes, std::uint64_t source_lines);
  void set_token_histogram(const std::vector<StyioToken*>& tokens);
  // MIGRATION-NEEDED: M-PARSER-02 (docs/rollups/MIGRATION-LEDGER.md)
  // legacy_fallback_statements and nightly_internal_legacy_bridges are
  // dropped from this signature once the parser engine migration completes.
  void set_parser_route_stats(
    std::uint64_t nightly_subset_statements,
    std::uint64_t nightly_declined_statements,
    std::uint64_t legacy_fallback_statements,
    std::uint64_t nightly_internal_legacy_bridges);
  void set_async_scheduler_stats(
    std::int64_t enabled,
    std::int64_t worker_count,
    std::int64_t ready_queue_kind,
    std::int64_t active_tasks,
    std::int64_t ready_tasks,
    std::int64_t spawned_tasks,
    std::int64_t enqueued_tasks,
    std::int64_t started_tasks,
    std::int64_t completed_tasks,
    std::int64_t pulled_tasks,
    std::int64_t released_tasks,
    std::int64_t fast_ready_pulls,
    std::int64_t blocking_pulls,
    std::int64_t failed_pulls,
    std::int64_t invalid_pulls,
    std::int64_t max_queue_depth,
    std::int64_t queue_capacity,
    std::int64_t queue_current_depth,
    std::int64_t queue_accepted_pushes,
    std::int64_t queue_pops,
    std::int64_t queue_pressure_events,
    std::int64_t queue_producer_waits,
    std::int64_t queue_consumer_waits,
    std::int64_t queue_close_wake_ups,
    std::int64_t queue_closed);
  void add_counter(std::string name, std::int64_t value);
  void mark_status(std::string status, std::string detail = std::string());

  std::string to_json() const;
  bool write(std::string* error_message = nullptr);

  static std::string default_output_path_for_source(const std::string& source_path);

private:
  struct PhaseRecord
  {
    std::string name;
    std::int64_t duration_ns = 0;
  };

  struct ParserRouteRecord
  {
    bool present = false;
    std::uint64_t nightly_subset_statements = 0;
    std::uint64_t nightly_declined_statements = 0;
    // MIGRATION-NEEDED: M-PARSER-02 (docs/rollups/MIGRATION-LEDGER.md)
    std::uint64_t legacy_fallback_statements = 0;
    std::uint64_t nightly_internal_legacy_bridges = 0;
  };

  struct AsyncSchedulerRecord
  {
    bool present = false;
    std::int64_t enabled = 0;
    std::int64_t worker_count = 0;
    std::int64_t ready_queue_kind = 0;
    std::int64_t active_tasks = 0;
    std::int64_t ready_tasks = 0;
    std::int64_t spawned_tasks = 0;
    std::int64_t enqueued_tasks = 0;
    std::int64_t started_tasks = 0;
    std::int64_t completed_tasks = 0;
    std::int64_t pulled_tasks = 0;
    std::int64_t released_tasks = 0;
    std::int64_t fast_ready_pulls = 0;
    std::int64_t blocking_pulls = 0;
    std::int64_t failed_pulls = 0;
    std::int64_t invalid_pulls = 0;
    std::int64_t max_queue_depth = 0;
    std::int64_t queue_capacity = 0;
    std::int64_t queue_current_depth = 0;
    std::int64_t queue_accepted_pushes = 0;
    std::int64_t queue_pops = 0;
    std::int64_t queue_pressure_events = 0;
    std::int64_t queue_producer_waits = 0;
    std::int64_t queue_consumer_waits = 0;
    std::int64_t queue_close_wake_ups = 0;
    std::int64_t queue_closed = 0;
  };

  bool enabled_ = false;
  bool written_ = false;
  std::string file_path_;
  std::string parser_engine_;
  std::string output_path_;
  std::string status_ = "failed";
  std::string status_detail_;
  std::int64_t started_unix_ms_ = 0;
  std::chrono::steady_clock::time_point started_at_;
  std::vector<PhaseRecord> phases_;
  std::map<std::string, std::int64_t> counters_;
  std::map<std::string, std::uint64_t> token_histogram_;
  ParserRouteRecord parser_route_;
  AsyncSchedulerRecord async_scheduler_;
};

} // namespace styio::profiler
