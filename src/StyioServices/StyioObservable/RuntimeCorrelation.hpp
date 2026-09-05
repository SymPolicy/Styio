#pragma once
#ifndef STYIO_OBSERVABLE_RUNTIME_CORRELATION_HPP_
#define STYIO_OBSERVABLE_RUNTIME_CORRELATION_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace styio::observable {

inline constexpr int kRuntimeEventsSchemaVersion = 2;
inline constexpr std::string_view kRuntimeEventsContractName =
  "styio.observable.runtime-events";
inline constexpr std::string_view kRuntimeEventsStability = "incubating";
inline constexpr std::string_view kRuntimeEventsPrivacyProfile = "strict";
inline constexpr std::string_view kRuntimeEventsClockUnit = "ns";
inline constexpr std::string_view kInstanceIdPrefix = "i2_";
inline constexpr std::string_view kEventIdPrefix = "r2_";
inline constexpr std::string_view kWaitIdPrefix = "w2_";
inline constexpr std::string_view kExecutionIdPrefix = "x2_";

inline constexpr std::uint32_t kDefaultLaneCapacity = 256;
inline constexpr std::uint32_t kDefaultPriorityReserved = 32;
inline constexpr std::uint32_t kDefaultDrainBatch = 64;
inline constexpr std::uint32_t kMinLaneCapacity = 64;
inline constexpr std::uint32_t kMaxLaneCapacity = 4096;
inline constexpr std::uint32_t kDefaultSamplingNumerator = 1;
inline constexpr std::uint32_t kDefaultSamplingDenominator = 16;
inline constexpr std::uint64_t kDefaultSamplingSeed = 0;
inline constexpr std::uint32_t kSamplingMixVersion = 1;
inline constexpr std::uint32_t kControllerLane = 0;
inline constexpr std::uint32_t kInvalidDescriptorIndex = 0xffffffffu;

enum class ObservationMode : std::uint8_t
{
  Disabled = 0,
  Aggregate = 1,
  Sampled = 2,
  Detailed = 3,
};

enum class EventKind : std::uint8_t
{
  SessionCapability = 0,
  SessionSummary = 1,
  CompileStarted = 2,
  CompileFinished = 3,
  CompileFailed = 4,
  UnitEntered = 5,
  UnitExited = 6,
  UnitTestStarted = 7,
  UnitTestFinished = 8,
  TransitionFired = 9,
  StateChanged = 10,
  DiagnosticEmitted = 11,
  RunStarted = 12,
  RunFinished = 13,
  ThreadSpawned = 14,
  ThreadExited = 15,
  LogEmitted = 16,
  TaskCreated = 17,
  TaskEnqueued = 18,
  TaskDequeued = 19,
  TaskStarted = 20,
  TaskCompleted = 21,
  TaskFailed = 22,
  TaskResultConsumed = 23,
  TaskReleased = 24,
  QueuePressure = 25,
  QueueClosed = 26,
  WaitBegin = 27,
  WaitEnd = 28,
  AggregateShard = 29,
  CancellationRequested = 30,
  CancellationCompleted = 31,
  CooperativeSuspend = 32,
  CooperativeResume = 33,
};

enum class EventFamily : std::uint8_t
{
  Session = 0,
  Controller = 1,
  TaskLifecycle = 2,
  Queue = 3,
  Wait = 4,
  Causal = 5,
  Aggregate = 6,
  Detail = 7,
};

enum class EventPriority : std::uint8_t
{
  Detail = 0,
  Lifecycle = 1,
};

enum class CorrelationStatus : std::uint8_t
{
  Correlated = 0,
  RuntimeOnly = 1,
  Unavailable = 2,
};

enum class SiteRole : std::uint8_t
{
  Task = 0,
  Await = 1,
  RuntimeOnly = 2,
};

enum class CausalKind : std::uint8_t
{
  Spawn = 0,
  Enqueue = 1,
  Dispatch = 2,
  Completion = 3,
  Wake = 4,
  Failure = 5,
  Cancellation = 6,
  BackpressureRelief = 7,
};

enum class WaitReason : std::uint8_t
{
  Runnable = 0,
  Cooperative = 1,
  Io = 2,
  Resource = 3,
  Task = 4,
  Backpressure = 5,
  Timer = 6,
  Cancellation = 7,
  Unknown = 8,
};

enum class WaitResolution : std::uint8_t
{
  Ready = 0,
  Completed = 1,
  Failed = 2,
  Cancelled = 3,
  TimedOut = 4,
  Closed = 5,
  Unknown = 6,
};

enum class Disposition : std::uint8_t
{
  Emitted = 0,
  Aggregated = 1,
  SampledOut = 2,
  BufferDropped = 3,
  ExporterDropped = 4,
};

enum class Completeness : std::uint8_t
{
  Complete = 0,
  PartialSampling = 1,
  PartialAggregation = 2,
  PartialBufferLoss = 3,
  PartialExportLoss = 4,
  PartialUnresolved = 5,
  PartialDisabled = 6,
  PartialExporterFailure = 7,
};

enum class RuntimeEventReject : std::uint8_t
{
  None = 0,
  UnsupportedVersion = 1,
  UnknownVersion = 2,
  Malformed = 3,
  UnsupportedCapability = 4,
  UnsupportedMode = 5,
  InvalidBounds = 6,
};

inline constexpr std::array<std::string_view, 8> kRuntimeObservationCapabilities{
  "task-lifecycle",
  "scheduler-queue",
  "wait-runnable",
  "wait-task",
  "wait-backpressure",
  "controller-events",
  "loss-accounting",
  "strict-privacy",
};

inline constexpr std::array<std::string_view, 6> kRuntimeObservationUnavailableCapabilities{
  "cancellation",
  "cooperative-suspend",
  "wait-cooperative",
  "wait-io",
  "wait-resource",
  "wait-timer",
};

struct SamplingSpec
{
  std::uint32_t numerator = kDefaultSamplingNumerator;
  std::uint32_t denominator = kDefaultSamplingDenominator;
  std::uint64_t seed = kDefaultSamplingSeed;
};

struct EventReference
{
  std::uint32_t lane = kControllerLane;
  std::uint64_t seq = 0;
};

struct PackedInstance
{
  std::uint32_t lane = kControllerLane;
  std::uint64_t seq = 0;
};

struct CausalEdge
{
  CausalKind kind = CausalKind::Spawn;
  EventReference event;
  PackedInstance subject;
};

struct WaitFields
{
  PackedInstance wait_id;
  PackedInstance waiter;
  PackedInstance subject;
  WaitReason reason = WaitReason::Unknown;
  WaitResolution resolution = WaitResolution::Unknown;
  bool subject_present = false;
  bool duration_valid = false;
  std::uint64_t duration_ns = 0;
};

struct FamilyAccounting
{
  std::uint64_t observed = 0;
  std::uint64_t emitted = 0;
  std::uint64_t aggregated = 0;
  std::uint64_t sampled_out = 0;
  std::uint64_t buffer_dropped = 0;
  std::uint64_t exporter_dropped = 0;
  std::uint64_t summary_updates = 0;
};

struct SessionAccounting
{
  std::array<FamilyAccounting, 8> families{};
  std::uint64_t high_water_occupancy = 0;
  std::uint32_t lane_capacity = kDefaultLaneCapacity;
  std::uint32_t priority_reserved = kDefaultPriorityReserved;
  std::uint32_t producer_lanes = 0;
  bool exporter_failed = false;
  Completeness completeness = Completeness::Complete;
};

struct RuntimeEvent
{
  EventKind kind = EventKind::SessionCapability;
  EventFamily family = EventFamily::Session;
  EventPriority priority = EventPriority::Lifecycle;
  CorrelationStatus correlation = CorrelationStatus::RuntimeOnly;
  SiteRole role = SiteRole::RuntimeOnly;
  std::uint32_t descriptor_index = kInvalidDescriptorIndex;
  std::string snapshot_id;
  std::string site_id;
  PackedInstance instance;
  EventReference event_ref;
  std::uint64_t monotonic_ns = 0;
  std::vector<CausalEdge> causes;
  std::optional<WaitFields> wait;
  std::string unit_id;
  std::string test_name;
  std::string intent;
  std::string phase;
  std::string operation;
  std::string diagnostic_code;
  std::string stream;
  std::string from_phase;
  std::string to_phase;
  std::string final_phase;
  bool success = false;
  bool success_present = false;
  bool executed = false;
  bool executed_present = false;
  std::uint32_t queue_depth = 0;
  std::uint32_t queue_capacity = 0;
  std::uint64_t count = 0;
  std::uint64_t duration_ns = 0;
};

struct SessionCapability
{
  int schema_version = kRuntimeEventsSchemaVersion;
  ObservationMode mode = ObservationMode::Disabled;
  int snapshot_schema = 1;
  std::string snapshot_id;
  std::string execution_id;
  std::string privacy_profile{kRuntimeEventsPrivacyProfile};
  std::uint32_t producer_lanes = 0;
  std::uint32_t lane_capacity = kDefaultLaneCapacity;
  std::uint32_t priority_reserved = kDefaultPriorityReserved;
  std::uint32_t drain_batch = kDefaultDrainBatch;
  SamplingSpec sampling;
  std::string clock_unit{kRuntimeEventsClockUnit};
  std::vector<std::string> supported_capabilities;
  std::vector<std::string> active_capabilities;
  std::vector<std::string> unavailable_capabilities;
};

struct SessionSummary
{
  SessionAccounting accounting;
  ObservationMode mode = ObservationMode::Disabled;
  std::string execution_id;
};

struct ParseIssue
{
  bool ok = false;
  std::string error;
  RuntimeEvent event;
  SessionCapability capability;
  SessionSummary summary;
  std::string record_kind;
};

bool observation_mode_from_text(std::string_view text, ObservationMode& out) noexcept;
std::string_view observation_mode_text(ObservationMode mode) noexcept;
std::string_view event_kind_text(EventKind kind) noexcept;
bool event_kind_from_text(std::string_view text, EventKind& out) noexcept;
std::string_view event_family_text(EventFamily family) noexcept;
std::string_view correlation_status_text(CorrelationStatus status) noexcept;
std::string_view site_role_text(SiteRole role) noexcept;
std::string_view causal_kind_text(CausalKind kind) noexcept;
bool causal_kind_from_text(std::string_view text, CausalKind& out) noexcept;
std::string_view wait_reason_text(WaitReason reason) noexcept;
bool wait_reason_from_text(std::string_view text, WaitReason& out) noexcept;
std::string_view wait_resolution_text(WaitResolution resolution) noexcept;
bool wait_resolution_from_text(std::string_view text, WaitResolution& out) noexcept;
std::string_view completeness_text(Completeness value) noexcept;
std::string_view runtime_event_reject_subcode(RuntimeEventReject code) noexcept;
std::string_view runtime_event_reject_message(RuntimeEventReject code) noexcept;

EventFamily family_for_event(EventKind kind) noexcept;
EventPriority priority_for_event(EventKind kind) noexcept;
bool capability_is_supported(std::string_view name) noexcept;
bool capability_is_unavailable(std::string_view name) noexcept;
bool lane_capacity_is_valid(std::uint32_t capacity) noexcept;

std::string encode_packed_id(std::string_view prefix, PackedInstance id);
std::string encode_event_id(EventReference ref);
bool parse_packed_id(std::string_view text, std::string_view prefix, PackedInstance& out) noexcept;
bool parse_event_id(std::string_view text, EventReference& out) noexcept;

std::uint64_t sampling_mix(
  std::string_view snapshot_id,
  std::string_view site_id,
  PackedInstance instance,
  EventFamily family,
  std::uint64_t seed
) noexcept;
bool sampling_selects_detail(
  std::string_view snapshot_id,
  std::string_view site_id,
  PackedInstance instance,
  EventFamily family,
  const SamplingSpec& spec
) noexcept;

bool accounting_conserved(const FamilyAccounting& row) noexcept;
bool accounting_conserved(const SessionAccounting& accounting) noexcept;
Completeness completeness_from_accounting(const SessionAccounting& accounting) noexcept;

std::string serialize_session_capability(const SessionCapability& capability);
std::string serialize_runtime_event(const RuntimeEvent& event);
std::string serialize_session_summary(const SessionSummary& summary);
ParseIssue parse_runtime_record(std::string_view json);
std::string runtime_events_machine_info_json();
std::string runtime_events_machine_info_json_nano();

std::vector<std::string_view> privacy_canaries();
bool payload_contains_privacy_canary(std::string_view text) noexcept;

} // namespace styio::observable

#endif
