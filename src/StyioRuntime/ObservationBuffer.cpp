#include "ObservationBuffer.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace styio::runtime::observation {
namespace {

using styio::observable::Disposition;
using styio::observable::RuntimeEvent;
using styio::observable::SessionSummary;
using styio::observable::kControllerLane;
using styio::observable::kInvalidDescriptorIndex;

struct Slot
{
  Record record;
  std::atomic<std::uint64_t> sequence{0};
};

struct AggregateCell
{
  std::uint64_t count = 0;
  std::uint64_t duration_ns = 0;
};

// Per-lane ledger rows are atomic: the owning producer increments them at emit
// time while the drain owner (a different thread) adjusts emitted /
// exporter_dropped when the sink fails, and accounting_snapshot reads them
// from any thread.
struct AtomicFamilyRow
{
  std::atomic<std::uint64_t> observed{0};
  std::atomic<std::uint64_t> emitted{0};
  std::atomic<std::uint64_t> aggregated{0};
  std::atomic<std::uint64_t> sampled_out{0};
  std::atomic<std::uint64_t> buffer_dropped{0};
  std::atomic<std::uint64_t> exporter_dropped{0};
  std::atomic<std::uint64_t> summary_updates{0};
};

struct Lane
{
  std::uint32_t id = 0;
  std::uint32_t mask = 0;
  std::uint32_t capacity = 0;
  std::uint32_t reserved = 0;
  std::unique_ptr<Slot[]> slots;
  std::atomic<std::uint64_t> head{0};
  std::atomic<std::uint64_t> tail{0};
  std::uint64_t instance_seq = 0;
  std::uint64_t event_seq = 0;
  std::uint64_t wait_seq = 0;
  std::atomic<std::uint64_t> occupancy_high{0};
  std::array<AtomicFamilyRow, 8> local{};
  std::vector<AggregateCell> aggregates;
};

struct Session
{
  SessionConfig config;
  std::uint32_t generation = 1;
  std::vector<Descriptor> descriptors;
  std::vector<std::unique_ptr<Lane>> lanes;
  std::uint32_t next_drain_lane = 0;
  std::atomic<bool> exporter_failed{false};
  // Wait episodes whose begin entered the stream without a matching end (a
  // wait begin and its end may be emitted on different lanes, so this is a
  // session-level count, not per-lane).
  std::atomic<std::uint64_t> open_waits{0};
  bool capability_written = false;
  bool summary_written = false;
  std::mutex drain_mu;
  // The single drain owner: exactly one thread per enabled session visits all
  // lanes fairly so producers never wait for telemetry and buffered records
  // reach the sink while the program still runs.
  std::atomic<bool> drain_stop{false};
  std::thread drain_thread;

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session() = default;
  ~Session();
};

thread_local std::uint32_t t_lane = kControllerLane;
std::unique_ptr<Session> g_session;
std::atomic<bool> g_active{false};

SessionCapability capability_record_from(const Session& session) {
  SessionCapability cap;
  cap.mode = session.config.mode;
  cap.snapshot_id = session.config.snapshot_id;
  cap.execution_id = session.config.execution_id;
  cap.producer_lanes = session.config.producer_lanes;
  cap.lane_capacity = session.config.lane_capacity;
  cap.priority_reserved = session.config.priority_reserved;
  cap.drain_batch = session.config.drain_batch;
  cap.sampling = session.config.sampling;
  for (const auto name : styio::observable::kRuntimeObservationCapabilities) {
    cap.supported_capabilities.emplace_back(name);
    if (session.config.mode != ObservationMode::Disabled) {
      cap.active_capabilities.emplace_back(name);
    }
  }
  for (const auto name : styio::observable::kRuntimeObservationUnavailableCapabilities) {
    cap.unavailable_capabilities.emplace_back(name);
  }
  return cap;
}

std::uint64_t default_clock() {
  return static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch())
      .count());
}

Lane* lane_by_id(Session& session, std::uint32_t id) {
  // Never alias an out-of-range lane onto another producer's SPSC ring: two
  // producers on one lane would corrupt the ring. The session is sized to
  // cover every scheduler worker plus the controller, so an out-of-range id
  // means a misconfigured direct caller; dropping is safe, aliasing is not.
  if (id >= session.lanes.size() || session.lanes[id] == nullptr) {
    return nullptr;
  }
  return session.lanes[id].get();
}

std::size_t aggregate_index(std::uint32_t descriptor_index, EventFamily family, std::size_t descriptor_count) {
  const std::size_t families = 8;
  if (descriptor_index == kInvalidDescriptorIndex
      || static_cast<std::size_t>(descriptor_index) >= descriptor_count) {
    return descriptor_count * families;
  }
  return static_cast<std::size_t>(descriptor_index) * families + static_cast<std::size_t>(family);
}

void account(Lane& lane, EventFamily family, Disposition disposition, bool count_observed) {
  auto& row = lane.local[static_cast<std::size_t>(family)];
  if (count_observed) {
    row.observed.fetch_add(1, std::memory_order_relaxed);
  }
  switch (disposition) {
    case Disposition::Emitted:
      row.emitted.fetch_add(1, std::memory_order_relaxed);
      break;
    case Disposition::Aggregated:
      row.aggregated.fetch_add(1, std::memory_order_relaxed);
      break;
    case Disposition::SampledOut:
      row.sampled_out.fetch_add(1, std::memory_order_relaxed);
      break;
    case Disposition::BufferDropped:
      row.buffer_dropped.fetch_add(1, std::memory_order_relaxed);
      break;
    case Disposition::ExporterDropped:
      row.exporter_dropped.fetch_add(1, std::memory_order_relaxed);
      break;
  }
}

void account(Lane& lane, EventFamily family, Disposition disposition) {
  account(lane, family, disposition, true);
}

bool try_push(Lane& lane, const Record& record) {
  const std::uint64_t pos = lane.tail.load(std::memory_order_relaxed);
  const std::uint64_t head = lane.head.load(std::memory_order_acquire);
  const std::uint64_t occupancy = pos - head;
  if (occupancy >= lane.capacity) {
    return false;
  }
  if (record.priority == EventPriority::Detail
      && occupancy >= static_cast<std::uint64_t>(lane.capacity - lane.reserved)) {
    return false;
  }
  Slot& slot = lane.slots[pos & lane.mask];
  slot.record = record;
  slot.sequence.store(pos + 1, std::memory_order_release);
  lane.tail.store(pos + 1, std::memory_order_release);
  if (occupancy + 1 > lane.occupancy_high.load(std::memory_order_relaxed)) {
    lane.occupancy_high.store(occupancy + 1, std::memory_order_relaxed);
  }
  return true;
}

bool try_pop(Lane& lane, Record& out) {
  const std::uint64_t pos = lane.head.load(std::memory_order_relaxed);
  const std::uint64_t tail = lane.tail.load(std::memory_order_acquire);
  if (pos == tail) {
    return false;
  }
  Slot& slot = lane.slots[pos & lane.mask];
  if (slot.sequence.load(std::memory_order_acquire) != pos + 1) {
    return false;
  }
  out = slot.record;
  lane.head.store(pos + 1, std::memory_order_release);
  return true;
}

bool write_line(Session& session, std::string_view line) {
  if (!session.config.sink) {
    return false;
  }
  if (session.exporter_failed.load(std::memory_order_relaxed)) {
    return false;
  }
  std::string payload;
  payload.reserve(line.size() + 1);
  payload.append(line.data(), line.size());
  payload.push_back('\n');
  // Sink-failure isolation: a throwing or failing exporter must never escape
  // into the drain owner or an application thread.
  try {
    if (!session.config.sink(payload)) {
      session.exporter_failed.store(true, std::memory_order_relaxed);
      return false;
    }
  } catch (...) {
    session.exporter_failed.store(true, std::memory_order_relaxed);
    return false;
  }
  return true;
}

RuntimeEvent expand(const Session& session, const Record& record) {
  RuntimeEvent event;
  event.kind = record.kind;
  event.family = record.family;
  event.priority = record.priority;
  event.correlation = record.correlation;
  event.role = record.role;
  event.descriptor_index = record.descriptor_index;
  event.instance = record.instance;
  event.event_ref = record.event_ref;
  event.monotonic_ns = record.monotonic_ns;
  event.queue_depth = record.queue_depth;
  event.queue_capacity = record.queue_capacity;
  event.count = record.count;
  event.duration_ns = record.duration_ns;
  if (record.descriptor_index < session.descriptors.size()) {
    const Descriptor& desc = session.descriptors[record.descriptor_index];
    event.snapshot_id = desc.snapshot_id;
    event.site_id = desc.site_id;
    event.role = desc.role;
    event.correlation = CorrelationStatus::Correlated;
  } else if (record.correlation == CorrelationStatus::Correlated) {
    event.snapshot_id = session.config.snapshot_id;
  }
  if (record.has_cause) {
    event.causes.push_back({record.cause_kind, record.cause_event, record.cause_subject});
  }
  if (record.kind == EventKind::WaitBegin || record.kind == EventKind::WaitEnd) {
    styio::observable::WaitFields wait;
    wait.wait_id = record.wait_id;
    wait.waiter = record.waiter;
    wait.subject = record.subject;
    wait.reason = record.wait_reason;
    wait.resolution = record.wait_resolution;
    wait.subject_present = record.subject_present;
    wait.duration_valid = record.duration_valid;
    wait.duration_ns = record.duration_ns;
    event.wait = wait;
    event.instance = record.waiter;
  }
  return event;
}

void flush_aggregates(Session& session, Lane& lane) {
  const std::size_t families = 8;
  const std::size_t descriptor_count = session.descriptors.size();
  for (std::size_t i = 0; i < lane.aggregates.size(); ++i) {
    const AggregateCell& cell = lane.aggregates[i];
    if (cell.count == 0) {
      continue;
    }
    Record record;
    record.kind = EventKind::AggregateShard;
    record.family = EventFamily::Aggregate;
    record.priority = EventPriority::Detail;
    record.count = cell.count;
    record.duration_ns = cell.duration_ns;
    record.event_ref.lane = lane.id;
    record.event_ref.seq = ++lane.event_seq;
    if (i < descriptor_count * families) {
      record.descriptor_index = static_cast<std::uint32_t>(i / families);
      record.correlation = CorrelationStatus::Correlated;
      if (record.descriptor_index < session.descriptors.size()) {
        record.role = session.descriptors[record.descriptor_index].role;
      }
    } else {
      record.correlation = CorrelationStatus::RuntimeOnly;
    }
    const RuntimeEvent event = expand(session, record);
    (void)write_line(session, styio::observable::serialize_runtime_event(event));
  }
}

std::uint32_t
drain_lane_batch(Session& session, Lane& lane, std::uint32_t batch) {
  std::uint32_t taken = 0;
  Record record;
  while (taken < batch && try_pop(lane, record)) {
    ++taken;
    const RuntimeEvent event = expand(session, record);
    if (!write_line(session, styio::observable::serialize_runtime_event(event))) {
      // The producer incremented emitted before publishing the slot, so the
      // decrement below can never run ahead of that increment.
      auto& row = lane.local[static_cast<std::size_t>(record.family)];
      row.emitted.fetch_sub(1, std::memory_order_relaxed);
      account(lane, record.family, Disposition::ExporterDropped, false);
    }
  }
  return taken;
}

std::uint32_t
drain_batch_for(const Session& session) {
  return session.config.drain_batch == 0
    ? styio::observable::kDefaultDrainBatch
    : session.config.drain_batch;
}

void drain_locked(Session& session) {
  if (session.lanes.empty()) {
    return;
  }
  const std::uint32_t batch = drain_batch_for(session);
  for (std::size_t visited = 0; visited < session.lanes.size(); ++visited) {
    Lane& lane = *session.lanes[session.next_drain_lane];
    session.next_drain_lane = (session.next_drain_lane + 1) % static_cast<std::uint32_t>(session.lanes.size());
    (void)drain_lane_batch(session, lane, batch);
  }
}

void drain_all_locked(Session& session) {
  // Shutdown drain: keep visiting lanes until every ring is empty so no
  // sink-accepted record is left buffered (and silently lost) at session end.
  const std::uint32_t batch = drain_batch_for(session);
  for (;;) {
    std::uint64_t taken_total = 0;
    for (auto& lane_ptr : session.lanes) {
      if (lane_ptr != nullptr) {
        taken_total += drain_lane_batch(session, *lane_ptr, batch);
      }
    }
    if (taken_total == 0) {
      break;
    }
  }
}

void drain_loop(Session* session) {
  while (!session->drain_stop.load(std::memory_order_acquire)) {
    {
      std::lock_guard<std::mutex> lock(session->drain_mu);
      drain_locked(*session);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

Session::~Session() {
  drain_stop.store(true, std::memory_order_release);
  if (drain_thread.joinable()) {
    drain_thread.join();
  }
}

} // namespace

bool
session_active() noexcept {
  return g_active.load(std::memory_order_acquire);
}

bool
begin_session(SessionConfig config) {
  if (config.mode == ObservationMode::Disabled) {
    return false;
  }
  // One session at a time: replacing a live session would silently abandon
  // its buffered records and accounting.
  if (g_active.load(std::memory_order_acquire) || g_session) {
    return false;
  }
  if (!styio::observable::lane_capacity_is_valid(config.lane_capacity)) {
    config.lane_capacity = styio::observable::kDefaultLaneCapacity;
  }
  if (config.priority_reserved >= config.lane_capacity) {
    config.priority_reserved = styio::observable::kDefaultPriorityReserved;
  }
  if (config.producer_lanes == 0) {
    config.producer_lanes = 1;
  }
  if (!config.clock) {
    config.clock = default_clock;
  }
  auto session = std::make_unique<Session>();
  session->config = std::move(config);
  const std::uint32_t lane_count = session->config.producer_lanes + 1;
  session->lanes.reserve(lane_count);
  for (std::uint32_t i = 0; i < lane_count; ++i) {
    auto lane = std::make_unique<Lane>();
    lane->id = i;
    lane->capacity = session->config.lane_capacity;
    lane->mask = session->config.lane_capacity - 1;
    lane->reserved = session->config.priority_reserved;
    lane->slots.reset(new Slot[lane->capacity]);
    for (std::uint32_t slot = 0; slot < lane->capacity; ++slot) {
      lane->slots[slot].sequence.store(0, std::memory_order_relaxed);
    }
    // The explicit runtime-only aggregate bucket exists even before (or
    // without) descriptor registration, so aggregate mode never accounts a
    // folded fact it cannot retain.
    lane->aggregates.assign(1, AggregateCell{});
    session->lanes.push_back(std::move(lane));
  }
  SessionCapability cap = capability_record_from(*session);
  (void)write_line(*session, styio::observable::serialize_session_capability(cap));
  session->capability_written = true;
  g_session = std::move(session);
  g_active.store(true, std::memory_order_release);
  t_lane = kControllerLane;
  try {
    g_session->drain_thread = std::thread(drain_loop, g_session.get());
  } catch (...) {
    // If the drain owner cannot start, the session still drains completely at
    // end_session; observation degrades to buffered-at-exit instead of failing.
  }
  return true;
}

void
end_session() {
  if (!g_session) {
    g_active.store(false, std::memory_order_release);
    return;
  }
  // Stop hook-driven emissions first so the final drain observes a quiescent
  // session, then stop and join the drain owner before teardown.
  g_active.store(false, std::memory_order_release);
  g_session->drain_stop.store(true, std::memory_order_release);
  if (g_session->drain_thread.joinable()) {
    g_session->drain_thread.join();
  }
  std::lock_guard<std::mutex> lock(g_session->drain_mu);
  drain_all_locked(*g_session);
  for (auto& lane : g_session->lanes) {
    flush_aggregates(*g_session, *lane);
  }
  SessionSummary summary;
  summary.mode = g_session->config.mode;
  summary.execution_id = g_session->config.execution_id;
  summary.accounting = accounting_snapshot();
  summary.accounting.completeness =
    styio::observable::completeness_from_accounting(summary.accounting);
  // A wait begin whose end never entered the stream (e.g. a worker still
  // parked on the ready queue at session end) is an unresolved reference:
  // the summary must not claim the stream is complete. Loss and exporter
  // failure stay the stronger partial reasons.
  const std::uint64_t open_waits = g_session->open_waits.load(std::memory_order_relaxed);
  if (open_waits > 0
      && (summary.accounting.completeness == styio::observable::Completeness::Complete
          || summary.accounting.completeness == styio::observable::Completeness::PartialSampling
          || summary.accounting.completeness == styio::observable::Completeness::PartialAggregation)) {
    summary.accounting.completeness = styio::observable::Completeness::PartialUnresolved;
  }
  if (!g_session->exporter_failed.load(std::memory_order_relaxed)) {
    (void)write_line(*g_session, styio::observable::serialize_session_summary(summary));
    g_session->summary_written = true;
  }
  g_session.reset();
  t_lane = kControllerLane;
}

void
register_table(std::uint32_t generation, std::vector<Descriptor> descriptors) {
  if (!g_session) {
    return;
  }
  g_session->generation = generation == 0 ? 1 : generation;
  g_session->descriptors = std::move(descriptors);
  const std::size_t cells = g_session->descriptors.size() * 8 + 1;
  for (auto& lane : g_session->lanes) {
    lane->aggregates.assign(cells, AggregateCell{});
  }
}

bool
table_registered() noexcept {
  return g_session && !g_session->descriptors.empty();
}

std::uint32_t
table_generation() noexcept {
  return g_session ? g_session->generation : 0;
}

const Descriptor*
descriptor_at(std::uint32_t index) noexcept {
  if (!g_session || index >= g_session->descriptors.size()) {
    return nullptr;
  }
  return &g_session->descriptors[index];
}

void
set_thread_lane(std::uint32_t lane) noexcept {
  t_lane = lane;
}

std::uint32_t
thread_lane() noexcept {
  return t_lane;
}

PackedInstance
allocate_instance() {
  PackedInstance id;
  id.lane = t_lane;
  if (!session_active()) {
    return id;
  }
  Lane* lane = lane_by_id(*g_session, t_lane);
  if (lane == nullptr) {
    return id;
  }
  id.seq = ++lane->instance_seq;
  return id;
}

EventReference
next_event_ref() {
  EventReference ref;
  ref.lane = t_lane;
  if (!session_active()) {
    return ref;
  }
  Lane* lane = lane_by_id(*g_session, t_lane);
  if (lane == nullptr) {
    return ref;
  }
  ref.seq = ++lane->event_seq;
  return ref;
}

PackedInstance
next_wait_id() {
  PackedInstance id;
  id.lane = t_lane;
  if (!session_active()) {
    return id;
  }
  Lane* lane = lane_by_id(*g_session, t_lane);
  if (lane == nullptr) {
    return id;
  }
  id.seq = ++lane->wait_seq;
  return id;
}

bool
emit(Record record) {
  if (!session_active()) {
    return false;
  }
  Lane* lane = lane_by_id(*g_session, t_lane);
  if (lane == nullptr) {
    return false;
  }
  record.family = styio::observable::family_for_event(record.kind);
  record.priority = styio::observable::priority_for_event(record.kind);
  if (record.event_ref.seq == 0) {
    record.event_ref.lane = lane->id;
    record.event_ref.seq = ++lane->event_seq;
  }
  if (record.monotonic_ns == 0 && g_session->config.clock) {
    record.monotonic_ns = g_session->config.clock();
  }
  if (record.descriptor_index < g_session->descriptors.size()) {
    record.correlation = CorrelationStatus::Correlated;
    record.role = g_session->descriptors[record.descriptor_index].role;
  } else if (record.correlation == CorrelationStatus::Correlated) {
    record.correlation = CorrelationStatus::RuntimeOnly;
    record.descriptor_index = kInvalidDescriptorIndex;
  }
  const bool priority = record.priority == EventPriority::Lifecycle;
  auto& row = lane->local[static_cast<std::size_t>(record.family)];
  row.summary_updates.fetch_add(1, std::memory_order_relaxed);

  if (!priority && g_session->config.mode == ObservationMode::Aggregate) {
    const std::size_t index = aggregate_index(
      record.descriptor_index, record.family, g_session->descriptors.size());
    if (index < lane->aggregates.size()) {
      lane->aggregates[index].count += 1;
      lane->aggregates[index].duration_ns += record.duration_ns;
    }
    account(*lane, record.family, Disposition::Aggregated);
    return true;
  }

  if (!priority && g_session->config.mode == ObservationMode::Sampled) {
    std::string_view snapshot;
    std::string_view site;
    if (record.descriptor_index < g_session->descriptors.size()) {
      snapshot = g_session->descriptors[record.descriptor_index].snapshot_id;
      site = g_session->descriptors[record.descriptor_index].site_id;
    }
    if (!styio::observable::sampling_selects_detail(
          snapshot, site, record.instance, record.family, g_session->config.sampling)) {
      account(*lane, record.family, Disposition::SampledOut);
      return true;
    }
  }

  // Account the emitted disposition before publishing the slot so the drain
  // owner's export-failure adjustment can never observe a missing increment.
  account(*lane, record.family, Disposition::Emitted);
  if (!try_push(*lane, record)) {
    lane->local[static_cast<std::size_t>(record.family)].emitted.fetch_sub(
      1, std::memory_order_relaxed);
    account(*lane, record.family, Disposition::BufferDropped, false);
    return false;
  }
  if (record.kind == EventKind::WaitBegin) {
    g_session->open_waits.fetch_add(1, std::memory_order_relaxed);
  } else if (record.kind == EventKind::WaitEnd) {
    // Saturating decrement: an end whose begin was buffer-dropped must not
    // wrap the counter.
    auto& open = g_session->open_waits;
    std::uint64_t value = open.load(std::memory_order_relaxed);
    while (value > 0
           && !open.compare_exchange_weak(
             value, value - 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
    }
  }
  return true;
}

void
drain() {
  if (!g_session) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_session->drain_mu);
  drain_locked(*g_session);
}

SessionAccounting
accounting_snapshot() {
  SessionAccounting out;
  if (!g_session) {
    return out;
  }
  out.lane_capacity = g_session->config.lane_capacity;
  out.priority_reserved = g_session->config.priority_reserved;
  out.producer_lanes = g_session->config.producer_lanes;
  out.exporter_failed = g_session->exporter_failed.load(std::memory_order_relaxed);
  for (const auto& lane : g_session->lanes) {
    for (std::size_t i = 0; i < out.families.size(); ++i) {
      const auto& src = lane->local[i];
      auto& dst = out.families[i];
      dst.observed += src.observed.load(std::memory_order_relaxed);
      dst.emitted += src.emitted.load(std::memory_order_relaxed);
      dst.aggregated += src.aggregated.load(std::memory_order_relaxed);
      dst.sampled_out += src.sampled_out.load(std::memory_order_relaxed);
      dst.buffer_dropped += src.buffer_dropped.load(std::memory_order_relaxed);
      dst.exporter_dropped += src.exporter_dropped.load(std::memory_order_relaxed);
      dst.summary_updates += src.summary_updates.load(std::memory_order_relaxed);
    }
    const std::uint64_t high = lane->occupancy_high.load(std::memory_order_relaxed);
    if (high > out.high_water_occupancy) {
      out.high_water_occupancy = high;
    }
  }
  out.completeness = styio::observable::completeness_from_accounting(out);
  return out;
}

SessionCapability
capability_record() {
  SessionCapability cap;
  if (!g_session) {
    cap.mode = ObservationMode::Disabled;
    return cap;
  }
  return capability_record_from(*g_session);
}

std::string
execution_id() {
  return g_session ? g_session->config.execution_id : std::string();
}

ObservationMode
mode() noexcept {
  return g_session ? g_session->config.mode : ObservationMode::Disabled;
}

bool
exporter_failed() noexcept {
  return g_session && g_session->exporter_failed.load(std::memory_order_relaxed);
}

bool
allocated_runtime_state() noexcept {
  return g_session != nullptr;
}

bool
emit_event(const EmitBuilder& builder) {
  Record record;
  record.kind = builder.kind;
  record.descriptor_index = builder.descriptor_index;
  record.instance = builder.instance;
  record.event_ref = builder.event_ref;
  record.has_cause = builder.has_cause;
  record.cause_event = builder.cause;
  record.cause_subject = builder.cause_subject;
  record.cause_kind = builder.cause_kind;
  record.wait_id = builder.wait_id;
  record.waiter = builder.waiter;
  record.subject = builder.subject;
  record.wait_reason = builder.wait_reason;
  record.wait_resolution = builder.wait_resolution;
  record.subject_present = builder.subject_present;
  record.duration_valid = builder.duration_valid;
  record.duration_ns = builder.duration_ns;
  record.queue_depth = builder.queue_depth;
  record.queue_capacity = builder.queue_capacity;
  return emit(record);
}

} // namespace styio::runtime::observation
