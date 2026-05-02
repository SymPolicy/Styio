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
  void set_parser_route_stats(
    std::uint64_t nightly_subset_statements,
    std::uint64_t nightly_declined_statements,
    std::uint64_t legacy_fallback_statements,
    std::uint64_t nightly_internal_legacy_bridges);
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
    std::uint64_t legacy_fallback_statements = 0;
    std::uint64_t nightly_internal_legacy_bridges = 0;
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
};

} // namespace styio::profiler
