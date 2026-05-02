#include "StyioProfiler/FrontendProfiler.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace styio::profiler {

namespace {

std::string
json_escape(const std::string& value) {
  std::ostringstream out;
  for (unsigned char ch : value) {
    switch (ch) {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (ch < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(ch) << std::dec;
        }
        else {
          out << static_cast<char>(ch);
        }
        break;
    }
  }
  return out.str();
}

std::int64_t
unix_ms_now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::system_clock::now().time_since_epoch())
    .count();
}

std::int64_t
ns_between(
  std::chrono::steady_clock::time_point start,
  std::chrono::steady_clock::time_point end
) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

} // namespace

FrontendProfiler::PhaseScope::PhaseScope(FrontendProfiler* profiler, const char* name) :
    profiler_(profiler),
    active_(profiler_ != nullptr && profiler_->enabled()) {
  if (!active_) {
    return;
  }
  name_ = name != nullptr ? name : "";
  started_at_ = std::chrono::steady_clock::now();
}

FrontendProfiler::PhaseScope::PhaseScope(PhaseScope&& other) noexcept :
    profiler_(other.profiler_),
    name_(std::move(other.name_)),
    started_at_(other.started_at_),
    active_(other.active_) {
  other.profiler_ = nullptr;
  other.active_ = false;
}

FrontendProfiler::PhaseScope&
FrontendProfiler::PhaseScope::operator=(PhaseScope&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  if (active_ && profiler_ != nullptr) {
    profiler_->record_phase(name_, ns_between(started_at_, std::chrono::steady_clock::now()));
  }

  profiler_ = other.profiler_;
  name_ = std::move(other.name_);
  started_at_ = other.started_at_;
  active_ = other.active_;
  other.profiler_ = nullptr;
  other.active_ = false;
  return *this;
}

FrontendProfiler::PhaseScope::~PhaseScope() {
  if (!active_ || profiler_ == nullptr) {
    return;
  }
  profiler_->record_phase(name_, ns_between(started_at_, std::chrono::steady_clock::now()));
}

void
FrontendProfiler::enable(
  std::string file_path,
  std::string parser_engine,
  std::string output_path
) {
  enabled_ = true;
  written_ = false;
  file_path_ = std::move(file_path);
  parser_engine_ = std::move(parser_engine);
  output_path_ = std::move(output_path);
  status_ = "failed";
  status_detail_.clear();
  started_unix_ms_ = unix_ms_now();
  started_at_ = std::chrono::steady_clock::now();
  phases_.clear();
  counters_.clear();
  token_histogram_.clear();
  parser_route_ = ParserRouteRecord {};
}

bool
FrontendProfiler::enabled() const noexcept {
  return enabled_;
}

bool
FrontendProfiler::written() const noexcept {
  return written_;
}

const std::string&
FrontendProfiler::output_path() const noexcept {
  return output_path_;
}

FrontendProfiler::PhaseScope
FrontendProfiler::phase(const char* name) {
  return PhaseScope(this, name);
}

void
FrontendProfiler::record_phase(std::string name, std::int64_t duration_ns) {
  if (!enabled_) {
    return;
  }
  phases_.push_back(PhaseRecord{std::move(name), duration_ns});
}

void
FrontendProfiler::set_source_summary(std::uint64_t source_bytes, std::uint64_t source_lines) {
  add_counter("source_bytes", static_cast<std::int64_t>(source_bytes));
  add_counter("source_lines", static_cast<std::int64_t>(source_lines));
}

void
FrontendProfiler::set_token_histogram(const std::vector<StyioToken*>& tokens) {
  if (!enabled_) {
    return;
  }

  token_histogram_.clear();
  for (const StyioToken* token : tokens) {
    if (token == nullptr) {
      continue;
    }
    token_histogram_[StyioToken::getTokName(token->type)] += 1;
  }
  add_counter("token_count", static_cast<std::int64_t>(tokens.size()));
}

void
FrontendProfiler::set_parser_route_stats(
  std::uint64_t nightly_subset_statements,
  std::uint64_t nightly_declined_statements,
  std::uint64_t legacy_fallback_statements,
  std::uint64_t nightly_internal_legacy_bridges
) {
  if (!enabled_) {
    return;
  }
  parser_route_.present = true;
  parser_route_.nightly_subset_statements = nightly_subset_statements;
  parser_route_.nightly_declined_statements = nightly_declined_statements;
  parser_route_.legacy_fallback_statements = legacy_fallback_statements;
  parser_route_.nightly_internal_legacy_bridges = nightly_internal_legacy_bridges;
}

void
FrontendProfiler::add_counter(std::string name, std::int64_t value) {
  if (!enabled_) {
    return;
  }
  counters_[std::move(name)] = value;
}

void
FrontendProfiler::mark_status(std::string status, std::string detail) {
  if (!enabled_) {
    return;
  }
  status_ = std::move(status);
  status_detail_ = std::move(detail);
}

std::string
FrontendProfiler::to_json() const {
  const std::int64_t total_duration_ns =
    enabled_ ? ns_between(started_at_, std::chrono::steady_clock::now()) : 0;

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"tool\": \"styio-profiler\",\n";
  out << "  \"scope\": \"frontend\",\n";
  out << "  \"file\": \"" << json_escape(file_path_) << "\",\n";
  out << "  \"parser_engine\": \"" << json_escape(parser_engine_) << "\",\n";
  out << "  \"status\": \"" << json_escape(status_) << "\",\n";
  out << "  \"status_detail\": \"" << json_escape(status_detail_) << "\",\n";
  out << "  \"started_unix_ms\": " << started_unix_ms_ << ",\n";
  out << "  \"total_duration_ns\": " << total_duration_ns << ",\n";
  out << "  \"phases\": [";
  for (size_t i = 0; i < phases_.size(); ++i) {
    const PhaseRecord& phase = phases_[i];
    out << (i == 0 ? "\n" : ",\n")
        << "    {\"name\": \"" << json_escape(phase.name)
        << "\", \"duration_ns\": " << phase.duration_ns << "}";
  }
  if (!phases_.empty()) {
    out << "\n  ";
  }
  out << "],\n";

  out << "  \"counters\": {";
  size_t counter_index = 0;
  for (const auto& [name, value] : counters_) {
    out << (counter_index == 0 ? "\n" : ",\n")
        << "    \"" << json_escape(name) << "\": " << value;
    counter_index += 1;
  }
  if (!counters_.empty()) {
    out << "\n  ";
  }
  out << "},\n";

  out << "  \"token_histogram\": {";
  size_t token_index = 0;
  for (const auto& [name, count] : token_histogram_) {
    out << (token_index == 0 ? "\n" : ",\n")
        << "    \"" << json_escape(name) << "\": " << count;
    token_index += 1;
  }
  if (!token_histogram_.empty()) {
    out << "\n  ";
  }
  out << "},\n";

  out << "  \"parser_route\": ";
  if (parser_route_.present) {
    out << "{"
        << "\"nightly_subset_statements\": " << parser_route_.nightly_subset_statements
        << ", \"nightly_declined_statements\": " << parser_route_.nightly_declined_statements
        << ", \"legacy_fallback_statements\": " << parser_route_.legacy_fallback_statements
        << ", \"nightly_internal_legacy_bridges\": "
        << parser_route_.nightly_internal_legacy_bridges
        << "}\n";
  }
  else {
    out << "null\n";
  }
  out << "}\n";
  return out.str();
}

bool
FrontendProfiler::write(std::string* error_message) {
  if (!enabled_) {
    return true;
  }
  if (output_path_.empty()) {
    if (error_message != nullptr) {
      *error_message = "profile output path is empty";
    }
    return false;
  }

  std::error_code ec;
  const std::filesystem::path out_path(output_path_);
  const std::filesystem::path parent = out_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      if (error_message != nullptr) {
        *error_message = "cannot create profile output directory: " + parent.string();
      }
      return false;
    }
  }

  std::ofstream out(out_path, std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    if (error_message != nullptr) {
      *error_message = "cannot open profile output path: " + output_path_;
    }
    return false;
  }

  out << to_json();
  if (!out.good()) {
    if (error_message != nullptr) {
      *error_message = "cannot write profile output path: " + output_path_;
    }
    return false;
  }

  written_ = true;
  return true;
}

std::string
FrontendProfiler::default_output_path_for_source(const std::string& source_path) {
  if (source_path.empty()) {
    return "styio-profile.json";
  }
  return source_path + ".profile.json";
}

} // namespace styio::profiler
