#include "RuntimeEventSession.hpp"

#if !STYIO_NANO_BUILD

#include <algorithm>
#include <chrono>
#include <fstream>
#include <system_error>
#include <thread>
#include <tuple>

#include "StyioRuntime/ObservationBuffer.hpp"
#include "StyioRuntime/TaskWorkerCount.hpp"

namespace styio::cli::runtime_events {
namespace {

std::string make_execution_id() {
  static std::uint64_t counter = 1;
  styio::observable::PackedInstance id{0, counter++};
  return styio::observable::encode_packed_id(styio::observable::kExecutionIdPrefix, id);
}

bool append_file(const std::filesystem::path& path, std::string_view text, std::string& error) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error = "cannot create file parent directory: " + path.parent_path().string();
    return false;
  }
  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out.is_open()) {
    error = "cannot open file for appending: " + path.string();
    return false;
  }
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  out.flush();
  if (!out.good()) {
    error = "failed to append file: " + path.string();
    return false;
  }
  return true;
}

} // namespace

bool
Session::open(
  const styio::config::CompilePlanRequest& request,
  std::string& error_message
) {
  if (request.build_root.empty()) {
    return true;
  }
  path_ = request.build_root / "runtime-events.jsonl";
  execution_id_ = make_execution_id();
  mode_ = request.emit_runtime_observation
    ? request.runtime_observation_mode
    : styio::observable::ObservationMode::Disabled;
  std::error_code ec;
  if (std::filesystem::exists(path_, ec) && std::filesystem::is_directory(path_, ec)) {
    error_message = "cannot create runtime-events.jsonl: path is a directory";
    return false;
  }
  {
    std::ofstream probe(path_, std::ios::binary | std::ios::app);
    if (!probe.is_open()) {
      error_message = "cannot open file for writing: " + path_.string();
      return false;
    }
  }
  enabled_ = true;

  if (mode_ == styio::observable::ObservationMode::Disabled) {
    styio::observable::SessionCapability cap;
    cap.mode = mode_;
    cap.execution_id = execution_id_;
    cap.producer_lanes = 0;
    cap.lane_capacity = request.runtime_observation_lane_capacity;
    cap.priority_reserved = request.runtime_observation_priority_reserved;
    cap.sampling = request.runtime_observation_sampling;
    for (const auto name : styio::observable::kRuntimeObservationCapabilities) {
      cap.supported_capabilities.emplace_back(name);
    }
    for (const auto name : styio::observable::kRuntimeObservationUnavailableCapabilities) {
      cap.unavailable_capabilities.emplace_back(name);
    }
    if (!append_line(styio::observable::serialize_session_capability(cap) + "\n")) {
      error_message = "failed to append file: " + path_.string();
      enabled_ = false;
      return false;
    }
    return true;
  }

  styio::runtime::observation::SessionConfig config;
  config.mode = mode_;
  config.execution_id = execution_id_;
  // One SPSC ring per scheduler worker plus the controller lane: the session
  // must cover the scheduler's worker count (including a STYIO_TASK_THREADS
  // override), so an explicit producer_lanes below that floor is raised to it
  // and the capability record advertises the value actually used.
  const std::uint32_t worker_lanes = styio::runtime::configured_task_worker_count();
  config.producer_lanes = request.runtime_observation_producer_lanes == 0
    ? worker_lanes
    : request.runtime_observation_producer_lanes;
  if (config.producer_lanes > 64) {
    config.producer_lanes = 64;
  }
  config.producer_lanes = std::max(config.producer_lanes, worker_lanes);
  config.lane_capacity = request.runtime_observation_lane_capacity;
  config.priority_reserved = request.runtime_observation_priority_reserved;
  config.sampling = request.runtime_observation_sampling;
  config.sink = [this](std::string_view line) {
    return append_line(line);
  };
  if (clock_) {
    config.clock = clock_;
  }
  // The capability record must name the exact correlation snapshot, which is
  // bound only when Sema's instrumentation table registers descriptors. Defer
  // the runtime session until then; controller events emitted meanwhile are
  // buffered and flushed immediately after the capability record.
  observation_deferred_ = true;
  deferred_config_ = std::move(config);
  return true;
}

void
Session::close() {
  if (!enabled_) {
    return;
  }
  if (observation_deferred_) {
    // Compilation ended before the snapshot binding (for example a failure):
    // start the session now so the stream still holds the capability record,
    // the buffered controller events, and a summary. snapshot_id stays null
    // because no snapshot was bound.
    observation_deferred_ = false;
    buffer_started_ = styio::runtime::observation::begin_session(std::move(deferred_config_));
    for (const auto& line : pending_lines_) {
      (void)append_line(line);
    }
    pending_lines_.clear();
  }
  if (buffer_started_) {
    styio::runtime::observation::end_session();
    buffer_started_ = false;
  } else {
    styio::observable::SessionSummary summary;
    summary.mode = mode_;
    summary.execution_id = execution_id_;
    summary.accounting.completeness = styio::observable::Completeness::PartialDisabled;
    (void)append_line(styio::observable::serialize_session_summary(summary) + "\n");
  }
  enabled_ = false;
}

void
Session::emit_controller(styio::observable::RuntimeEvent event) {
  if (!enabled_) {
    return;
  }
  event.family = styio::observable::family_for_event(event.kind);
  event.priority = styio::observable::priority_for_event(event.kind);
  event.correlation = styio::observable::CorrelationStatus::RuntimeOnly;
  event.role = styio::observable::SiteRole::RuntimeOnly;
  if (event.event_ref.seq == 0) {
    event.event_ref.lane = styio::observable::kControllerLane;
    event.event_ref.seq = next_seq_++;
  }
  if (event.monotonic_ns == 0) {
    event.monotonic_ns = now_ns();
  }
  const std::string line = styio::observable::serialize_runtime_event(event) + "\n";
  if (observation_deferred_) {
    pending_lines_.push_back(line);
    return;
  }
  (void)append_line(line);
}

void
Session::register_descriptors(
  std::uint32_t generation,
  const std::string& snapshot_id,
  const std::vector<std::tuple<std::string, std::string, std::uint8_t>>& descriptors
) {
  if (observation_deferred_) {
    observation_deferred_ = false;
    deferred_config_.snapshot_id = snapshot_id;
    buffer_started_ = styio::runtime::observation::begin_session(std::move(deferred_config_));
    for (const auto& line : pending_lines_) {
      (void)append_line(line);
    }
    pending_lines_.clear();
  }
  std::vector<styio::runtime::observation::Descriptor> table;
  table.reserve(descriptors.size());
  for (const auto& item : descriptors) {
    styio::runtime::observation::Descriptor desc;
    desc.snapshot_id = std::get<0>(item);
    desc.site_id = std::get<1>(item);
    desc.role = static_cast<styio::observable::SiteRole>(std::get<2>(item));
    table.push_back(std::move(desc));
  }
  styio::runtime::observation::register_table(generation, std::move(table));
}

bool
Session::append_line(std::string_view line) {
  std::lock_guard<std::mutex> lock(mu_);
  std::string error;
  return append_file(path_, line, error);
}

std::uint64_t
Session::now_ns() const {
  if (clock_) {
    return clock_();
  }
  return static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch())
      .count());
}

} // namespace styio::cli::runtime_events

#endif
