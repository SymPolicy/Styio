#include "RuntimeCorrelation.hpp"

#include "JsonSupport.hpp"

#include <algorithm>
#include <cstring>

namespace styio::observable {
namespace {

using json_detail::CompactJson;
using json_detail::JsonValue;
using json_detail::parse_json;

struct NamedKind
{
  std::string_view text;
  EventKind kind;
};

constexpr NamedKind kEventKinds[] = {
  {"session.capability", EventKind::SessionCapability},
  {"session.summary", EventKind::SessionSummary},
  {"compile.started", EventKind::CompileStarted},
  {"compile.finished", EventKind::CompileFinished},
  {"compile.failed", EventKind::CompileFailed},
  {"unit.entered", EventKind::UnitEntered},
  {"unit.exited", EventKind::UnitExited},
  {"unit.test.started", EventKind::UnitTestStarted},
  {"unit.test.finished", EventKind::UnitTestFinished},
  {"transition.fired", EventKind::TransitionFired},
  {"state.changed", EventKind::StateChanged},
  {"diagnostic.emitted", EventKind::DiagnosticEmitted},
  {"run.started", EventKind::RunStarted},
  {"run.finished", EventKind::RunFinished},
  {"thread.spawned", EventKind::ThreadSpawned},
  {"thread.exited", EventKind::ThreadExited},
  {"log.emitted", EventKind::LogEmitted},
  {"task.created", EventKind::TaskCreated},
  {"task.enqueued", EventKind::TaskEnqueued},
  {"task.dequeued", EventKind::TaskDequeued},
  {"task.started", EventKind::TaskStarted},
  {"task.completed", EventKind::TaskCompleted},
  {"task.failed", EventKind::TaskFailed},
  {"task.result_consumed", EventKind::TaskResultConsumed},
  {"task.released", EventKind::TaskReleased},
  {"queue.pressure", EventKind::QueuePressure},
  {"queue.closed", EventKind::QueueClosed},
  {"wait.begin", EventKind::WaitBegin},
  {"wait.end", EventKind::WaitEnd},
  {"aggregate.shard", EventKind::AggregateShard},
  {"cancellation.requested", EventKind::CancellationRequested},
  {"cancellation.completed", EventKind::CancellationCompleted},
  {"cooperative.suspend", EventKind::CooperativeSuspend},
  {"cooperative.resume", EventKind::CooperativeResume},
};

constexpr std::string_view kCausalKinds[] = {
  "spawn",
  "enqueue",
  "dispatch",
  "completion",
  "wake",
  "failure",
  "cancellation",
  "backpressure_relief",
};

constexpr std::string_view kWaitReasons[] = {
  "runnable",
  "cooperative",
  "io",
  "resource",
  "task",
  "backpressure",
  "timer",
  "cancellation",
  "unknown",
};

constexpr std::string_view kWaitResolutions[] = {
  "ready",
  "completed",
  "failed",
  "cancelled",
  "timed_out",
  "closed",
  "unknown",
};

constexpr std::string_view kModes[] = {"disabled", "aggregate", "sampled", "detailed"};
constexpr std::string_view kFamilies[] = {
  "session", "controller", "task_lifecycle", "queue", "wait", "causal", "aggregate", "detail"};
constexpr std::string_view kCorrelation[] = {"correlated", "runtime_only", "unavailable"};
constexpr std::string_view kRoles[] = {"task", "await", "runtime_only"};
constexpr std::string_view kCompleteness[] = {
  "complete",
  "partial/sampling",
  "partial/aggregation",
  "partial/buffer_loss",
  "partial/export_loss",
  "partial/unresolved",
  "partial/disabled",
  "partial/exporter_failure",
};

constexpr std::string_view kPrivacyCanaries[] = {
  "CANARY_SECRET_sk-live-test",
  "/absolute/canary/path",
  "canary.example.host",
  "203.0.113.77",
  "0xCANARYADDR",
  "raw-canary-value",
  "canary-source-label",
  "diagnostic canary message",
  "STYIO_CANARY_ENV",
};

void emit_string_array(CompactJson& json, const std::vector<std::string>& values) {
  json.begin_array();
  for (const auto& value : values) {
    json.string_value(value);
  }
  json.end_array();
}

void emit_packed_id(CompactJson& json, std::string_view prefix, PackedInstance id) {
  json.string_value(encode_packed_id(prefix, id));
}

std::uint64_t fnv_mix(std::uint64_t hash, std::string_view bytes) noexcept {
  constexpr std::uint64_t kPrime = 1099511628211ull;
  for (unsigned char ch : bytes) {
    hash ^= static_cast<std::uint64_t>(ch);
    hash *= kPrime;
  }
  hash ^= kSamplingMixVersion;
  hash *= kPrime;
  return hash;
}

const JsonValue* field(const JsonValue& object, std::string_view name) {
  return object.field(name);
}

bool copy_string(const JsonValue* value, std::string& out) {
  if (value == nullptr || value->as_string() == nullptr) {
    return false;
  }
  out = *value->as_string();
  return true;
}

bool copy_int(const JsonValue* value, long long& out) {
  if (value == nullptr || value->kind != JsonValue::Kind::Int) {
    return false;
  }
  out = value->int_value;
  return true;
}

} // namespace

bool
observation_mode_from_text(std::string_view text, ObservationMode& out) noexcept {
  for (std::size_t i = 0; i < 4; ++i) {
    if (kModes[i] == text) {
      out = static_cast<ObservationMode>(i);
      return true;
    }
  }
  return false;
}

std::string_view
observation_mode_text(ObservationMode mode) noexcept {
  const auto index = static_cast<std::size_t>(mode);
  return index < 4 ? kModes[index] : kModes[0];
}

std::string_view
event_kind_text(EventKind kind) noexcept {
  for (const auto& entry : kEventKinds) {
    if (entry.kind == kind) {
      return entry.text;
    }
  }
  return "unknown";
}

bool
event_kind_from_text(std::string_view text, EventKind& out) noexcept {
  for (const auto& entry : kEventKinds) {
    if (entry.text == text) {
      out = entry.kind;
      return true;
    }
  }
  return false;
}

std::string_view
event_family_text(EventFamily family) noexcept {
  const auto index = static_cast<std::size_t>(family);
  return index < 8 ? kFamilies[index] : kFamilies[7];
}

std::string_view
correlation_status_text(CorrelationStatus status) noexcept {
  const auto index = static_cast<std::size_t>(status);
  return index < 3 ? kCorrelation[index] : kCorrelation[2];
}

std::string_view
site_role_text(SiteRole role) noexcept {
  const auto index = static_cast<std::size_t>(role);
  return index < 3 ? kRoles[index] : kRoles[2];
}

std::string_view
causal_kind_text(CausalKind kind) noexcept {
  const auto index = static_cast<std::size_t>(kind);
  return index < 8 ? kCausalKinds[index] : kCausalKinds[0];
}

bool
causal_kind_from_text(std::string_view text, CausalKind& out) noexcept {
  for (std::size_t i = 0; i < 8; ++i) {
    if (kCausalKinds[i] == text) {
      out = static_cast<CausalKind>(i);
      return true;
    }
  }
  return false;
}

std::string_view
wait_reason_text(WaitReason reason) noexcept {
  const auto index = static_cast<std::size_t>(reason);
  return index < 9 ? kWaitReasons[index] : kWaitReasons[8];
}

bool
wait_reason_from_text(std::string_view text, WaitReason& out) noexcept {
  for (std::size_t i = 0; i < 9; ++i) {
    if (kWaitReasons[i] == text) {
      out = static_cast<WaitReason>(i);
      return true;
    }
  }
  return false;
}

std::string_view
wait_resolution_text(WaitResolution resolution) noexcept {
  const auto index = static_cast<std::size_t>(resolution);
  return index < 7 ? kWaitResolutions[index] : kWaitResolutions[6];
}

bool
wait_resolution_from_text(std::string_view text, WaitResolution& out) noexcept {
  for (std::size_t i = 0; i < 7; ++i) {
    if (kWaitResolutions[i] == text) {
      out = static_cast<WaitResolution>(i);
      return true;
    }
  }
  return false;
}

std::string_view
completeness_text(Completeness value) noexcept {
  const auto index = static_cast<std::size_t>(value);
  return index < 8 ? kCompleteness[index] : kCompleteness[6];
}

std::string_view
runtime_event_reject_subcode(RuntimeEventReject code) noexcept {
  switch (code) {
    case RuntimeEventReject::UnsupportedVersion:
      return "runtime_events_unsupported_version";
    case RuntimeEventReject::UnknownVersion:
      return "runtime_events_unknown_version";
    case RuntimeEventReject::Malformed:
      return "runtime_observation_malformed";
    case RuntimeEventReject::UnsupportedCapability:
      return "runtime_observation_unsupported_capability";
    case RuntimeEventReject::UnsupportedMode:
      return "runtime_observation_unsupported_mode";
    case RuntimeEventReject::InvalidBounds:
      return "runtime_observation_invalid_bounds";
    case RuntimeEventReject::None:
      break;
  }
  return "";
}

std::string_view
runtime_event_reject_message(RuntimeEventReject code) noexcept {
  switch (code) {
    case RuntimeEventReject::UnsupportedVersion:
      return "runtime-events version 1 is not supported; request version 2";
    case RuntimeEventReject::UnknownVersion:
      return "unsupported runtime-events version";
    case RuntimeEventReject::Malformed:
      return "compile-plan emit.runtime_observation is malformed";
    case RuntimeEventReject::UnsupportedCapability:
      return "unsupported required runtime observation capability";
    case RuntimeEventReject::UnsupportedMode:
      return "unsupported runtime observation mode";
    case RuntimeEventReject::InvalidBounds:
      return "runtime observation buffer bounds are invalid";
    case RuntimeEventReject::None:
      break;
  }
  return "";
}

EventFamily
family_for_event(EventKind kind) noexcept {
  switch (kind) {
    case EventKind::SessionCapability:
    case EventKind::SessionSummary:
      return EventFamily::Session;
    case EventKind::TaskCreated:
    case EventKind::TaskEnqueued:
    case EventKind::TaskDequeued:
    case EventKind::TaskStarted:
    case EventKind::TaskCompleted:
    case EventKind::TaskFailed:
    case EventKind::TaskResultConsumed:
    case EventKind::TaskReleased:
    case EventKind::CancellationRequested:
    case EventKind::CancellationCompleted:
    case EventKind::CooperativeSuspend:
    case EventKind::CooperativeResume:
      return EventFamily::TaskLifecycle;
    case EventKind::QueuePressure:
    case EventKind::QueueClosed:
      return EventFamily::Queue;
    case EventKind::WaitBegin:
    case EventKind::WaitEnd:
      return EventFamily::Wait;
    case EventKind::AggregateShard:
      return EventFamily::Aggregate;
    default:
      return EventFamily::Controller;
  }
}

EventPriority
priority_for_event(EventKind kind) noexcept {
  switch (kind) {
    case EventKind::TaskDequeued:
    case EventKind::QueuePressure:
    case EventKind::AggregateShard:
      return EventPriority::Detail;
    default:
      return EventPriority::Lifecycle;
  }
}

bool
capability_is_supported(std::string_view name) noexcept {
  for (const auto capability : kRuntimeObservationCapabilities) {
    if (capability == name) {
      return true;
    }
  }
  return false;
}

bool
capability_is_unavailable(std::string_view name) noexcept {
  for (const auto capability : kRuntimeObservationUnavailableCapabilities) {
    if (capability == name) {
      return true;
    }
  }
  return false;
}

bool
lane_capacity_is_valid(std::uint32_t capacity) noexcept {
  if (capacity < kMinLaneCapacity || capacity > kMaxLaneCapacity) {
    return false;
  }
  return (capacity & (capacity - 1u)) == 0u;
}

std::string
encode_packed_id(std::string_view prefix, PackedInstance id) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(prefix.size() + 16);
  out.append(prefix.data(), prefix.size());
  const std::uint64_t packed =
    (static_cast<std::uint64_t>(id.lane & 0xffffu) << 48) | (id.seq & 0xffffffffffffull);
  for (int shift = 60; shift >= 0; shift -= 4) {
    out.push_back(kHex[(packed >> shift) & 0xfu]);
  }
  return out;
}

std::string
encode_event_id(EventReference ref) {
  PackedInstance id{ref.lane, ref.seq};
  return encode_packed_id(kEventIdPrefix, id);
}

bool
parse_packed_id(std::string_view text, std::string_view prefix, PackedInstance& out) noexcept {
  if (text.size() != prefix.size() + 16 || text.substr(0, prefix.size()) != prefix) {
    return false;
  }
  std::uint64_t packed = 0;
  for (char ch : text.substr(prefix.size())) {
    packed <<= 4;
    if (ch >= '0' && ch <= '9') {
      packed |= static_cast<std::uint64_t>(ch - '0');
    } else if (ch >= 'a' && ch <= 'f') {
      packed |= static_cast<std::uint64_t>(ch - 'a' + 10);
    } else {
      return false;
    }
  }
  out.lane = static_cast<std::uint32_t>((packed >> 48) & 0xffffu);
  out.seq = packed & 0xffffffffffffull;
  return true;
}

bool
parse_event_id(std::string_view text, EventReference& out) noexcept {
  PackedInstance id;
  if (!parse_packed_id(text, kEventIdPrefix, id)) {
    return false;
  }
  out.lane = id.lane;
  out.seq = id.seq;
  return true;
}

std::uint64_t
sampling_mix(
  std::string_view snapshot_id,
  std::string_view site_id,
  PackedInstance instance,
  EventFamily family,
  std::uint64_t seed
) noexcept {
  std::uint64_t hash = 14695981039346656037ull ^ seed;
  hash = fnv_mix(hash, snapshot_id);
  hash = fnv_mix(hash, site_id);
  const std::string instance_bytes = encode_packed_id(kInstanceIdPrefix, instance);
  hash = fnv_mix(hash, instance_bytes);
  const unsigned char family_byte = static_cast<unsigned char>(family);
  hash = fnv_mix(hash, std::string_view(reinterpret_cast<const char*>(&family_byte), 1));
  return hash;
}

bool
sampling_selects_detail(
  std::string_view snapshot_id,
  std::string_view site_id,
  PackedInstance instance,
  EventFamily family,
  const SamplingSpec& spec
) noexcept {
  if (spec.denominator == 0 || spec.numerator == 0) {
    return false;
  }
  if (spec.numerator >= spec.denominator) {
    return true;
  }
  const std::uint64_t mix = sampling_mix(snapshot_id, site_id, instance, family, spec.seed);
  const std::uint32_t bucket = static_cast<std::uint32_t>(mix);
  return static_cast<std::uint64_t>(bucket) * spec.denominator
    < static_cast<std::uint64_t>(spec.numerator) << 32;
}

bool
accounting_conserved(const FamilyAccounting& row) noexcept {
  return row.observed
    == row.emitted + row.aggregated + row.sampled_out + row.buffer_dropped + row.exporter_dropped;
}

bool
accounting_conserved(const SessionAccounting& accounting) noexcept {
  for (const auto& row : accounting.families) {
    if (!accounting_conserved(row)) {
      return false;
    }
  }
  return true;
}

Completeness
completeness_from_accounting(const SessionAccounting& accounting) noexcept {
  if (accounting.exporter_failed) {
    return Completeness::PartialExporterFailure;
  }
  std::uint64_t buffer = 0;
  std::uint64_t exported = 0;
  std::uint64_t sampled = 0;
  std::uint64_t aggregated = 0;
  for (const auto& row : accounting.families) {
    buffer += row.buffer_dropped;
    exported += row.exporter_dropped;
    sampled += row.sampled_out;
    aggregated += row.aggregated;
  }
  if (buffer != 0) {
    return Completeness::PartialBufferLoss;
  }
  if (exported != 0) {
    return Completeness::PartialExportLoss;
  }
  if (sampled != 0) {
    return Completeness::PartialSampling;
  }
  if (aggregated != 0) {
    return Completeness::PartialAggregation;
  }
  return Completeness::Complete;
}

std::string
serialize_session_capability(const SessionCapability& capability) {
  CompactJson json;
  json.begin_object();
  json.key("contract");
  json.string_value(kRuntimeEventsContractName);
  json.key("schema_version");
  json.integer_value(capability.schema_version);
  json.key("stability");
  json.string_value(kRuntimeEventsStability);
  json.key("record_kind");
  json.string_value("session.capability");
  json.key("event_kind");
  json.string_value("session.capability");
  json.key("mode");
  json.string_value(observation_mode_text(capability.mode));
  json.key("snapshot_schema");
  json.integer_value(capability.snapshot_schema);
  json.key("snapshot_id");
  if (capability.snapshot_id.empty()) {
    json.null_value();
  } else {
    json.string_value(capability.snapshot_id);
  }
  json.key("execution_id");
  json.string_value(capability.execution_id);
  json.key("privacy_profile");
  json.string_value(capability.privacy_profile);
  json.key("producer_lanes");
  json.integer_value(capability.producer_lanes);
  json.key("lane_capacity");
  json.integer_value(capability.lane_capacity);
  json.key("priority_reserved");
  json.integer_value(capability.priority_reserved);
  json.key("drain_batch");
  json.integer_value(capability.drain_batch);
  json.key("sampling");
  json.begin_object();
  json.key("numerator");
  json.integer_value(capability.sampling.numerator);
  json.key("denominator");
  json.integer_value(capability.sampling.denominator);
  json.key("seed");
  json.integer_value(static_cast<long long>(capability.sampling.seed));
  json.end_object();
  json.key("clock_unit");
  json.string_value(capability.clock_unit);
  json.key("supported_capabilities");
  emit_string_array(json, capability.supported_capabilities);
  json.key("active_capabilities");
  emit_string_array(json, capability.active_capabilities);
  json.key("unavailable_capabilities");
  emit_string_array(json, capability.unavailable_capabilities);
  json.end_object();
  return json.buf;
}

std::string
serialize_runtime_event(const RuntimeEvent& event) {
  CompactJson json;
  json.begin_object();
  json.key("contract");
  json.string_value(kRuntimeEventsContractName);
  json.key("schema_version");
  json.integer_value(kRuntimeEventsSchemaVersion);
  json.key("record_kind");
  json.string_value("event");
  json.key("event_kind");
  json.string_value(event_kind_text(event.kind));
  json.key("family");
  json.string_value(event_family_text(event.family));
  json.key("priority");
  json.string_value(event.priority == EventPriority::Lifecycle ? "lifecycle" : "detail");
  json.key("correlation_status");
  json.string_value(correlation_status_text(event.correlation));
  json.key("role");
  json.string_value(site_role_text(event.role));
  json.key("snapshot_id");
  if (event.correlation == CorrelationStatus::Correlated) {
    json.string_value(event.snapshot_id);
  } else {
    json.null_value();
  }
  json.key("site_id");
  if (event.correlation == CorrelationStatus::Correlated) {
    json.string_value(event.site_id);
  } else {
    json.null_value();
  }
  json.key("instance_id");
  if (event.instance.seq == 0 && event.instance.lane == 0
      && event.kind != EventKind::WaitBegin && event.kind != EventKind::WaitEnd
      && event.family != EventFamily::TaskLifecycle) {
    json.null_value();
  } else {
    emit_packed_id(json, kInstanceIdPrefix, event.instance);
  }
  json.key("event_id");
  json.string_value(encode_event_id(event.event_ref));
  json.key("monotonic_ns");
  json.integer_value(static_cast<long long>(event.monotonic_ns));
  json.key("causes");
  json.begin_array();
  for (const auto& cause : event.causes) {
    json.begin_object();
    json.key("kind");
    json.string_value(causal_kind_text(cause.kind));
    json.key("event_id");
    json.string_value(encode_event_id(cause.event));
    json.key("subject_instance");
    json.string_value(encode_packed_id(kInstanceIdPrefix, cause.subject));
    json.end_object();
  }
  json.end_array();
  json.key("wait");
  if (!event.wait.has_value()) {
    json.null_value();
  } else {
    const WaitFields& wait = *event.wait;
    json.begin_object();
    json.key("wait_id");
    json.string_value(encode_packed_id(kWaitIdPrefix, wait.wait_id));
    json.key("waiter_instance");
    json.string_value(encode_packed_id(kInstanceIdPrefix, wait.waiter));
    json.key("subject_instance");
    if (wait.subject_present) {
      json.string_value(encode_packed_id(kInstanceIdPrefix, wait.subject));
    } else {
      json.null_value();
    }
    json.key("reason");
    json.string_value(wait_reason_text(wait.reason));
    json.key("resolution");
    if (event.kind == EventKind::WaitEnd) {
      json.string_value(wait_resolution_text(wait.resolution));
    } else {
      json.null_value();
    }
    json.key("duration_ns");
    if (wait.duration_valid) {
      json.integer_value(static_cast<long long>(wait.duration_ns));
    } else {
      json.null_value();
    }
    json.end_object();
  }
  if (!event.unit_id.empty()) {
    json.key("unit_id");
    json.string_value(event.unit_id);
  }
  if (!event.test_name.empty()) {
    json.key("test_name");
    json.string_value(event.test_name);
  }
  if (!event.intent.empty()) {
    json.key("intent");
    json.string_value(event.intent);
  }
  if (!event.phase.empty()) {
    json.key("phase");
    json.string_value(event.phase);
  }
  if (!event.operation.empty()) {
    json.key("operation");
    json.string_value(event.operation);
  }
  if (!event.diagnostic_code.empty()) {
    json.key("diagnostic_code");
    json.string_value(event.diagnostic_code);
  }
  if (!event.stream.empty()) {
    json.key("stream");
    json.string_value(event.stream);
  }
  if (!event.from_phase.empty()) {
    json.key("from_phase");
    json.string_value(event.from_phase);
  }
  if (!event.to_phase.empty()) {
    json.key("to_phase");
    json.string_value(event.to_phase);
  }
  if (!event.final_phase.empty()) {
    json.key("final_phase");
    json.string_value(event.final_phase);
  }
  if (event.success_present) {
    json.key("success");
    json.bool_value(event.success);
  }
  if (event.executed_present) {
    json.key("executed");
    json.bool_value(event.executed);
  }
  if (event.kind == EventKind::QueuePressure || event.kind == EventKind::QueueClosed
      || event.kind == EventKind::WaitBegin || event.kind == EventKind::WaitEnd) {
    json.key("queue_depth");
    json.integer_value(event.queue_depth);
    json.key("queue_capacity");
    json.integer_value(event.queue_capacity);
  }
  if (event.kind == EventKind::AggregateShard) {
    json.key("count");
    json.integer_value(static_cast<long long>(event.count));
    json.key("duration_ns");
    json.integer_value(static_cast<long long>(event.duration_ns));
  }
  json.end_object();
  return json.buf;
}

std::string
serialize_session_summary(const SessionSummary& summary) {
  CompactJson json;
  json.begin_object();
  json.key("contract");
  json.string_value(kRuntimeEventsContractName);
  json.key("schema_version");
  json.integer_value(kRuntimeEventsSchemaVersion);
  json.key("record_kind");
  json.string_value("session.summary");
  json.key("event_kind");
  json.string_value("session.summary");
  json.key("mode");
  json.string_value(observation_mode_text(summary.mode));
  json.key("execution_id");
  json.string_value(summary.execution_id);
  json.key("completeness");
  json.string_value(completeness_text(summary.accounting.completeness));
  json.key("exporter_failed");
  json.bool_value(summary.accounting.exporter_failed);
  json.key("lane_capacity");
  json.integer_value(summary.accounting.lane_capacity);
  json.key("priority_reserved");
  json.integer_value(summary.accounting.priority_reserved);
  json.key("producer_lanes");
  json.integer_value(summary.accounting.producer_lanes);
  json.key("high_water_occupancy");
  json.integer_value(static_cast<long long>(summary.accounting.high_water_occupancy));
  json.key("families");
  json.begin_object();
  for (std::size_t i = 0; i < summary.accounting.families.size(); ++i) {
    const auto& row = summary.accounting.families[i];
    json.key(kFamilies[i]);
    json.begin_object();
    json.key("observed");
    json.integer_value(static_cast<long long>(row.observed));
    json.key("emitted");
    json.integer_value(static_cast<long long>(row.emitted));
    json.key("aggregated");
    json.integer_value(static_cast<long long>(row.aggregated));
    json.key("sampled_out");
    json.integer_value(static_cast<long long>(row.sampled_out));
    json.key("buffer_dropped");
    json.integer_value(static_cast<long long>(row.buffer_dropped));
    json.key("exporter_dropped");
    json.integer_value(static_cast<long long>(row.exporter_dropped));
    json.key("summary_updates");
    json.integer_value(static_cast<long long>(row.summary_updates));
    json.end_object();
  }
  json.end_object();
  json.end_object();
  return json.buf;
}

ParseIssue
parse_runtime_record(std::string_view json) {
  ParseIssue issue;
  JsonValue root;
  std::string error;
  if (!parse_json(json, root, error) || !root.is_object()) {
    issue.error = error.empty() ? "malformed runtime event json" : error;
    return issue;
  }
  std::string record_kind;
  copy_string(field(root, "record_kind"), record_kind);
  if (record_kind.empty()) {
    copy_string(field(root, "event_kind"), record_kind);
  }
  issue.record_kind = record_kind;
  long long schema = 0;
  if (!copy_int(field(root, "schema_version"), schema) || schema != kRuntimeEventsSchemaVersion) {
    issue.error = "unsupported runtime-events schema_version";
    return issue;
  }
  if (record_kind == "session.capability") {
    issue.capability.schema_version = kRuntimeEventsSchemaVersion;
    std::string mode;
    copy_string(field(root, "mode"), mode);
    (void)observation_mode_from_text(mode, issue.capability.mode);
    copy_string(field(root, "snapshot_id"), issue.capability.snapshot_id);
    copy_string(field(root, "execution_id"), issue.capability.execution_id);
    copy_string(field(root, "privacy_profile"), issue.capability.privacy_profile);
    long long lanes = 0;
    if (copy_int(field(root, "producer_lanes"), lanes)) {
      issue.capability.producer_lanes = static_cast<std::uint32_t>(lanes);
    }
    long long capacity = 0;
    if (copy_int(field(root, "lane_capacity"), capacity)) {
      issue.capability.lane_capacity = static_cast<std::uint32_t>(capacity);
    }
    const JsonValue* sampling = field(root, "sampling");
    if (sampling != nullptr && sampling->is_object()) {
      long long num = 0;
      long long den = 0;
      long long seed = 0;
      if (copy_int(sampling->field("numerator"), num)) {
        issue.capability.sampling.numerator = static_cast<std::uint32_t>(num);
      }
      if (copy_int(sampling->field("denominator"), den)) {
        issue.capability.sampling.denominator = static_cast<std::uint32_t>(den);
      }
      if (copy_int(sampling->field("seed"), seed)) {
        issue.capability.sampling.seed = static_cast<std::uint64_t>(seed);
      }
    }
    const JsonValue* supported = field(root, "supported_capabilities");
    if (supported != nullptr && supported->as_array() != nullptr) {
      for (const auto& item : *supported->as_array()) {
        if (item.as_string() != nullptr) {
          issue.capability.supported_capabilities.push_back(*item.as_string());
        }
      }
    }
    const JsonValue* unavailable = field(root, "unavailable_capabilities");
    if (unavailable != nullptr && unavailable->as_array() != nullptr) {
      for (const auto& item : *unavailable->as_array()) {
        if (item.as_string() != nullptr) {
          issue.capability.unavailable_capabilities.push_back(*item.as_string());
        }
      }
    }
    issue.ok = true;
    return issue;
  }
  if (record_kind == "session.summary") {
    copy_string(field(root, "execution_id"), issue.summary.execution_id);
    std::string mode;
    copy_string(field(root, "mode"), mode);
    (void)observation_mode_from_text(mode, issue.summary.mode);
    issue.ok = true;
    return issue;
  }
  std::string kind_text;
  copy_string(field(root, "event_kind"), kind_text);
  if (!event_kind_from_text(kind_text, issue.event.kind)) {
    issue.error = "unknown event_kind";
    return issue;
  }
  issue.event.family = family_for_event(issue.event.kind);
  issue.event.priority = priority_for_event(issue.event.kind);
  std::string correlation;
  copy_string(field(root, "correlation_status"), correlation);
  if (correlation == "correlated") {
    issue.event.correlation = CorrelationStatus::Correlated;
  } else if (correlation == "unavailable") {
    issue.event.correlation = CorrelationStatus::Unavailable;
  } else {
    issue.event.correlation = CorrelationStatus::RuntimeOnly;
  }
  std::string role;
  copy_string(field(root, "role"), role);
  if (role == "task") {
    issue.event.role = SiteRole::Task;
  } else if (role == "await") {
    issue.event.role = SiteRole::Await;
  } else {
    issue.event.role = SiteRole::RuntimeOnly;
  }
  copy_string(field(root, "snapshot_id"), issue.event.snapshot_id);
  copy_string(field(root, "site_id"), issue.event.site_id);
  std::string instance;
  if (copy_string(field(root, "instance_id"), instance)) {
    (void)parse_packed_id(instance, kInstanceIdPrefix, issue.event.instance);
  }
  std::string event_id;
  if (copy_string(field(root, "event_id"), event_id)) {
    (void)parse_event_id(event_id, issue.event.event_ref);
  }
  copy_string(field(root, "unit_id"), issue.event.unit_id);
  copy_string(field(root, "test_name"), issue.event.test_name);
  copy_string(field(root, "intent"), issue.event.intent);
  copy_string(field(root, "phase"), issue.event.phase);
  copy_string(field(root, "operation"), issue.event.operation);
  copy_string(field(root, "from_phase"), issue.event.from_phase);
  copy_string(field(root, "to_phase"), issue.event.to_phase);
  copy_string(field(root, "final_phase"), issue.event.final_phase);
  copy_string(field(root, "diagnostic_code"), issue.event.diagnostic_code);
  copy_string(field(root, "stream"), issue.event.stream);
  const JsonValue* causes = field(root, "causes");
  if (causes != nullptr && causes->as_array() != nullptr) {
    for (const auto& item : *causes->as_array()) {
      if (!item.is_object()) {
        continue;
      }
      CausalEdge edge;
      std::string kind;
      copy_string(item.field("kind"), kind);
      (void)causal_kind_from_text(kind, edge.kind);
      std::string cause_event;
      if (copy_string(item.field("event_id"), cause_event)) {
        (void)parse_event_id(cause_event, edge.event);
      }
      std::string subject;
      if (copy_string(item.field("subject_instance"), subject)) {
        (void)parse_packed_id(subject, kInstanceIdPrefix, edge.subject);
      }
      issue.event.causes.push_back(edge);
    }
  }
  const JsonValue* wait = field(root, "wait");
  if (wait != nullptr && wait->is_object()) {
    WaitFields fields;
    std::string wait_id;
    if (copy_string(wait->field("wait_id"), wait_id)) {
      (void)parse_packed_id(wait_id, kWaitIdPrefix, fields.wait_id);
    }
    std::string waiter;
    if (copy_string(wait->field("waiter_instance"), waiter)) {
      (void)parse_packed_id(waiter, kInstanceIdPrefix, fields.waiter);
    }
    std::string subject;
    if (copy_string(wait->field("subject_instance"), subject)) {
      fields.subject_present = parse_packed_id(subject, kInstanceIdPrefix, fields.subject);
    }
    std::string reason;
    copy_string(wait->field("reason"), reason);
    (void)wait_reason_from_text(reason, fields.reason);
    std::string resolution;
    if (copy_string(wait->field("resolution"), resolution)) {
      (void)wait_resolution_from_text(resolution, fields.resolution);
    }
    long long duration = 0;
    if (copy_int(wait->field("duration_ns"), duration)) {
      fields.duration_valid = true;
      fields.duration_ns = static_cast<std::uint64_t>(duration);
    }
    issue.event.wait = fields;
  }
  issue.ok = true;
  return issue;
}

std::string
runtime_events_machine_info_json() {
  return "{\"schema_versions\":[2],\"default_mode\":\"aggregate\",\"capabilities\":["
         "\"controller-events\",\"loss-accounting\",\"scheduler-queue\",\"strict-privacy\","
         "\"task-lifecycle\",\"wait-backpressure\",\"wait-runnable\",\"wait-task\"],"
         "\"unavailable_capabilities\":[\"cancellation\",\"cooperative-suspend\","
         "\"wait-cooperative\",\"wait-io\",\"wait-resource\",\"wait-timer\"]}";
}

std::string
runtime_events_machine_info_json_nano() {
  return "{\"schema_versions\":[],\"default_mode\":\"disabled\",\"capabilities\":[],"
         "\"unavailable_capabilities\":[]}";
}

std::vector<std::string_view>
privacy_canaries() {
  return {std::begin(kPrivacyCanaries), std::end(kPrivacyCanaries)};
}

bool
payload_contains_privacy_canary(std::string_view text) noexcept {
  for (const auto canary : kPrivacyCanaries) {
    if (text.find(canary) != std::string_view::npos) {
      return true;
    }
  }
  return false;
}

} // namespace styio::observable
