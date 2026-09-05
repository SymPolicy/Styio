#pragma once
#ifndef STYIO_RUNTIME_OBSERVATION_BUFFER_HPP_
#define STYIO_RUNTIME_OBSERVATION_BUFFER_HPP_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "StyioServices/StyioObservable/RuntimeCorrelation.hpp"

namespace styio::runtime::observation {

using styio::observable::CausalKind;
using styio::observable::Completeness;
using styio::observable::CorrelationStatus;
using styio::observable::EventFamily;
using styio::observable::EventKind;
using styio::observable::EventPriority;
using styio::observable::EventReference;
using styio::observable::ObservationMode;
using styio::observable::PackedInstance;
using styio::observable::SamplingSpec;
using styio::observable::SessionAccounting;
using styio::observable::SessionCapability;
using styio::observable::SiteRole;
using styio::observable::WaitReason;
using styio::observable::WaitResolution;

struct Descriptor
{
  std::string snapshot_id;
  std::string site_id;
  SiteRole role = SiteRole::Task;
};

struct Record
{
  EventKind kind = EventKind::TaskCreated;
  EventFamily family = EventFamily::TaskLifecycle;
  EventPriority priority = EventPriority::Lifecycle;
  CorrelationStatus correlation = CorrelationStatus::RuntimeOnly;
  SiteRole role = SiteRole::RuntimeOnly;
  std::uint32_t descriptor_index = styio::observable::kInvalidDescriptorIndex;
  PackedInstance instance;
  EventReference event_ref;
  EventReference cause_event;
  PackedInstance cause_subject;
  CausalKind cause_kind = CausalKind::Spawn;
  bool has_cause = false;
  PackedInstance wait_id;
  PackedInstance waiter;
  PackedInstance subject;
  WaitReason wait_reason = WaitReason::Unknown;
  WaitResolution wait_resolution = WaitResolution::Unknown;
  bool subject_present = false;
  bool duration_valid = false;
  std::uint64_t duration_ns = 0;
  std::uint64_t monotonic_ns = 0;
  std::uint32_t queue_depth = 0;
  std::uint32_t queue_capacity = 0;
  std::uint64_t count = 0;
};

using SinkFn = std::function<bool(std::string_view line)>;
using ClockFn = std::function<std::uint64_t()>;

struct SessionConfig
{
  ObservationMode mode = ObservationMode::Disabled;
  std::string execution_id;
  std::string snapshot_id;
  std::uint32_t producer_lanes = 1;
  std::uint32_t lane_capacity = styio::observable::kDefaultLaneCapacity;
  std::uint32_t priority_reserved = styio::observable::kDefaultPriorityReserved;
  std::uint32_t drain_batch = styio::observable::kDefaultDrainBatch;
  SamplingSpec sampling;
  SinkFn sink;
  ClockFn clock;
};

bool session_active() noexcept;
bool begin_session(SessionConfig config);
void end_session();
void register_table(std::uint32_t generation, std::vector<Descriptor> descriptors);
bool table_registered() noexcept;
std::uint32_t table_generation() noexcept;
const Descriptor* descriptor_at(std::uint32_t index) noexcept;

void set_thread_lane(std::uint32_t lane) noexcept;
std::uint32_t thread_lane() noexcept;

PackedInstance allocate_instance();
EventReference next_event_ref();
PackedInstance next_wait_id();

bool emit(Record record);
void drain();
SessionAccounting accounting_snapshot();
SessionCapability capability_record();
std::string execution_id();
ObservationMode mode() noexcept;
bool exporter_failed() noexcept;
bool allocated_runtime_state() noexcept;

struct EmitBuilder
{
  EventKind kind = EventKind::TaskCreated;
  std::uint32_t descriptor_index = styio::observable::kInvalidDescriptorIndex;
  PackedInstance instance;
  EventReference event_ref;
  EventReference cause;
  PackedInstance cause_subject;
  CausalKind cause_kind = CausalKind::Spawn;
  bool has_cause = false;
  PackedInstance wait_id;
  PackedInstance waiter;
  PackedInstance subject;
  WaitReason wait_reason = WaitReason::Unknown;
  WaitResolution wait_resolution = WaitResolution::Unknown;
  bool subject_present = false;
  bool duration_valid = false;
  std::uint64_t duration_ns = 0;
  std::uint32_t queue_depth = 0;
  std::uint32_t queue_capacity = 0;
};

bool emit_event(const EmitBuilder& builder);

} // namespace styio::runtime::observation

#endif
