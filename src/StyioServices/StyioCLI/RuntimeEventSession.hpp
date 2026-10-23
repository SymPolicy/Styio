#pragma once
#ifndef STYIO_CLI_RUNTIME_EVENT_SESSION_HPP_
#define STYIO_CLI_RUNTIME_EVENT_SESSION_HPP_

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "StyioServices/StyioConfig/CompilePlanContract.hpp"
#include "StyioServices/StyioConfig/NanoProfile.hpp"
#include "StyioServices/StyioObservable/RuntimeCorrelation.hpp"
#if !STYIO_NANO_BUILD
#include "StyioRuntime/ObservationBuffer.hpp"
#endif

namespace styio::cli::runtime_events {

inline std::string
machine_info_json() {
#if STYIO_NANO_BUILD
  return styio::observable::runtime_events_machine_info_json_nano();
#else
  return styio::observable::runtime_events_machine_info_json();
#endif
}

#if STYIO_NANO_BUILD
inline constexpr const char* kSupportedRuntimeEventsVersions = "[]";
#else
inline constexpr const char* kSupportedRuntimeEventsVersions = "[2]";
#endif

class Session
{
public:
  using Clock = std::function<std::uint64_t()>;

#if STYIO_NANO_BUILD
  bool open(const styio::config::CompilePlanRequest&, std::string&) { return true; }
  void close() {}
  bool enabled() const noexcept { return false; }
  const std::string& session_id() const noexcept { return execution_id_; }
  const std::filesystem::path& path() const noexcept { return path_; }
  styio::observable::ObservationMode mode() const noexcept {
    return styio::observable::ObservationMode::Disabled;
  }
  void emit_controller(styio::observable::RuntimeEvent) {}
  bool observation_enabled() const noexcept { return false; }
  void set_clock(Clock) {}
  void register_descriptors(
    std::uint32_t,
    const std::string&,
    const std::vector<std::tuple<std::string, std::string, std::uint8_t>>&
  ) {}
#else
  bool open(
    const styio::config::CompilePlanRequest& request,
    std::string& error_message
  );
  void close();
  bool enabled() const noexcept { return enabled_; }
  const std::string& session_id() const noexcept { return execution_id_; }
  const std::filesystem::path& path() const noexcept { return path_; }
  styio::observable::ObservationMode mode() const noexcept { return mode_; }
  void emit_controller(styio::observable::RuntimeEvent event);
  bool observation_enabled() const noexcept {
    return mode_ != styio::observable::ObservationMode::Disabled;
  }
  void set_clock(Clock clock) { clock_ = std::move(clock); }
  void register_descriptors(
    std::uint32_t generation,
    const std::string& snapshot_id,
    const std::vector<std::tuple<std::string, std::string, std::uint8_t>>& descriptors
  );
#endif

private:
#if !STYIO_NANO_BUILD
  bool append_line(std::string_view line);
  std::uint64_t now_ns() const;
  bool enabled_ = false;
  styio::observable::ObservationMode mode_ =
    styio::observable::ObservationMode::Disabled;
  std::uint64_t next_seq_ = 1;
  std::mutex mu_;
  Clock clock_;
  bool buffer_started_ = false;
  // Enabled sessions defer the runtime session (and its capability record,
  // which must name the exact correlation snapshot) until descriptor
  // registration binds the snapshot id. Controller events emitted meanwhile
  // are buffered and flushed immediately after the capability record.
  bool observation_deferred_ = false;
  styio::runtime::observation::SessionConfig deferred_config_;
  std::vector<std::string> pending_lines_;
#endif
  std::filesystem::path path_;
  std::string execution_id_;
};

} // namespace styio::cli::runtime_events

#endif
