#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "EnvTestUtil.hpp"
#include "StyioExtern/ExternLib.hpp"
#include "StyioRuntime/ObservationBuffer.hpp"
#include "StyioServices/StyioObservable/RuntimeCorrelation.hpp"

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif

namespace fs = std::filesystem;
namespace obs = styio::observable;
namespace rt = styio::runtime::observation;

namespace {

constexpr const char* kFixtureSnapshot = "s1_0123456789abcdef0123456789abcdef";
constexpr const char* kFixtureTaskSite = "n1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr const char* kFixtureAwaitSite = "n1_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

fs::path fixture_dir() {
  return fs::path(STYIO_SOURCE_DIR) / "tests/fixtures/observable-runtime-correlation/v2";
}

std::string read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

std::vector<std::string> split_lines(const std::string& text) {
  std::vector<std::string> lines;
  std::string cur;
  for (char ch : text) {
    if (ch == '\n') {
      if (!cur.empty()) {
        lines.push_back(cur);
      }
      cur.clear();
    } else {
      cur.push_back(ch);
    }
  }
  if (!cur.empty()) {
    lines.push_back(cur);
  }
  return lines;
}

struct CapturingSink
{
  std::mutex mu;
  std::vector<std::string> lines;
  std::atomic<int> calls{0};
  int fail_after = -1;

  bool operator()(std::string_view line) {
    const int n = calls.fetch_add(1) + 1;
    if (fail_after >= 0 && n > fail_after) {
      return false;
    }
    std::lock_guard<std::mutex> lock(mu);
    lines.emplace_back(line);
    return true;
  }

  std::string joined() {
    std::lock_guard<std::mutex> lock(mu);
    std::string out;
    for (const auto& line : lines) {
      out += line;
    }
    return out;
  }
};

struct SessionGuard
{
  ~SessionGuard() {
    if (rt::session_active() || rt::allocated_runtime_state()) {
      rt::end_session();
    }
  }
};

std::uint64_t& clock_counter() {
  static std::uint64_t value = 1000;
  return value;
}

std::uint64_t test_clock() {
  return clock_counter() += 10;
}

void reset_clock() {
  clock_counter() = 1000;
}

rt::SessionConfig make_config(
  obs::ObservationMode mode,
  CapturingSink& sink,
  std::uint32_t lanes = 1,
  std::uint32_t capacity = obs::kDefaultLaneCapacity,
  std::uint32_t reserved = obs::kDefaultPriorityReserved
) {
  rt::SessionConfig config;
  config.mode = mode;
  config.execution_id = "x2_00000000000000fe";
  config.snapshot_id = kFixtureSnapshot;
  config.producer_lanes = lanes;
  config.lane_capacity = capacity;
  config.priority_reserved = reserved;
  config.sampling.numerator = 1;
  config.sampling.denominator = 16;
  config.sampling.seed = 7;
  config.sink = [&sink](std::string_view line) { return sink(line); };
  config.clock = test_clock;
  return config;
}

void register_fixture_table() {
  rt::register_table(
    1,
    {
      {kFixtureSnapshot, kFixtureTaskSite, obs::SiteRole::Task},
      {kFixtureSnapshot, kFixtureAwaitSite, obs::SiteRole::Await},
    });
}

int64_t task_return_7(void*) {
  return 7;
}

int64_t task_sleep_then_9(void*) {
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  return 9;
}

int64_t task_throw_canary(void*) {
  throw std::runtime_error("diagnostic canary message");
}

std::vector<obs::ParseIssue> parse_jsonl(const std::string& text) {
  std::vector<obs::ParseIssue> out;
  for (const auto& line : split_lines(text)) {
    std::string_view view = line;
    while (!view.empty() && (view.back() == '\n' || view.back() == '\r')) {
      view.remove_suffix(1);
    }
    if (view.empty()) {
      continue;
    }
    out.push_back(obs::parse_runtime_record(view));
  }
  return out;
}

bool has_kind(const std::vector<obs::ParseIssue>& records, obs::EventKind kind) {
  for (const auto& record : records) {
    if (record.ok && record.event.kind == kind) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST(StyioObservableRuntime, DisabledModeAllocatesNoRuntimeState) {
  SessionGuard guard;
  rt::SessionConfig config;
  config.mode = obs::ObservationMode::Disabled;
  CapturingSink sink;
  config.sink = [&sink](std::string_view line) { return sink(line); };
  EXPECT_FALSE(rt::begin_session(config));
  EXPECT_FALSE(rt::session_active());
  EXPECT_FALSE(rt::allocated_runtime_state());
  EXPECT_EQ(rt::mode(), obs::ObservationMode::Disabled);
  EXPECT_TRUE(sink.lines.empty());
  rt::EmitBuilder created;
  created.kind = obs::EventKind::TaskCreated;
  EXPECT_FALSE(rt::emit_event(created));
  EXPECT_FALSE(rt::allocated_runtime_state());
}

TEST(StyioObservableRuntime, AcceptedSnapshotSitesRoundTripThroughLoweringAndRuntime) {
  SessionGuard guard;
  reset_clock();
  CapturingSink sink;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, sink)));
  register_fixture_table();
  StyioObservationDescriptor table[2] = {
    {kFixtureSnapshot, kFixtureTaskSite, 0},
    {kFixtureSnapshot, kFixtureAwaitSite, 1},
  };
  styio_observation_register_table(1, table, 2);

  const int64_t handle = styio_task_i64_spawn_observed(&task_return_7, nullptr, 0);
  ASSERT_NE(handle, 0);
  EXPECT_EQ(styio_task_i64_pull_observed(handle, 1), 7);
  styio_task_release(handle);
  rt::drain();
  rt::end_session();

  const auto records = parse_jsonl(sink.joined());
  std::unordered_set<std::string> instances;
  bool saw_task = false;
  bool saw_wait = false;
  for (const auto& record : records) {
    ASSERT_TRUE(record.ok) << record.error;
    if (!record.event.snapshot_id.empty() && record.event.correlation == obs::CorrelationStatus::Correlated) {
      EXPECT_EQ(record.event.snapshot_id, kFixtureSnapshot);
      EXPECT_TRUE(
        record.event.site_id == kFixtureTaskSite || record.event.site_id == kFixtureAwaitSite)
        << record.event.site_id;
      const std::string instance =
        obs::encode_packed_id(obs::kInstanceIdPrefix, record.event.instance);
      EXPECT_NE(instance.find("i2_"), std::string::npos);
      EXPECT_EQ(instance.find("h"), std::string::npos);
      instances.insert(instance);
    }
    if (record.event.kind == obs::EventKind::TaskCreated) {
      saw_task = true;
      EXPECT_EQ(record.event.correlation, obs::CorrelationStatus::Correlated);
      EXPECT_EQ(record.event.site_id, kFixtureTaskSite);
    }
    if (record.event.kind == obs::EventKind::WaitBegin
        || record.event.kind == obs::EventKind::WaitEnd) {
      saw_wait = true;
    }
  }
  EXPECT_TRUE(saw_task);
  EXPECT_TRUE(saw_wait);
  EXPECT_FALSE(instances.empty());
}

TEST(StyioObservableRuntime, TaskSchedulerEmitsOwnedLifecycleAndCausalEdges) {
  SessionGuard guard;
  reset_clock();
  CapturingSink sink;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, sink)));
  register_fixture_table();
  StyioObservationDescriptor table[1] = {{kFixtureSnapshot, kFixtureTaskSite, 0}};
  styio_observation_register_table(1, table, 1);

  const int64_t ok = styio_task_i64_spawn_observed(&task_return_7, nullptr, 0);
  EXPECT_EQ(styio_task_i64_pull(ok), 7);
  styio_task_release(ok);

  const int64_t failed = styio_task_i64_spawn_observed(&task_throw_canary, nullptr, 0);
  EXPECT_EQ(styio_task_i64_pull(failed), 0);
  styio_task_release(failed);

  rt::drain();
  rt::end_session();
  const auto records = parse_jsonl(sink.joined());
  EXPECT_TRUE(has_kind(records, obs::EventKind::TaskCreated));
  EXPECT_TRUE(has_kind(records, obs::EventKind::TaskEnqueued));
  EXPECT_TRUE(has_kind(records, obs::EventKind::TaskDequeued));
  EXPECT_TRUE(has_kind(records, obs::EventKind::TaskStarted));
  EXPECT_TRUE(has_kind(records, obs::EventKind::TaskCompleted));
  EXPECT_TRUE(has_kind(records, obs::EventKind::TaskFailed));
  EXPECT_TRUE(has_kind(records, obs::EventKind::TaskResultConsumed));
  EXPECT_TRUE(has_kind(records, obs::EventKind::TaskReleased));
  bool saw_spawn = false;
  bool saw_dispatch = false;
  bool saw_completion = false;
  bool saw_failure = false;
  for (const auto& record : records) {
    for (const auto& cause : record.event.causes) {
      saw_spawn = saw_spawn || cause.kind == obs::CausalKind::Spawn;
      saw_dispatch = saw_dispatch || cause.kind == obs::CausalKind::Dispatch;
      saw_completion = saw_completion || cause.kind == obs::CausalKind::Completion
        || cause.kind == obs::CausalKind::Wake;
      saw_failure = saw_failure || cause.kind == obs::CausalKind::Failure;
    }
  }
  EXPECT_TRUE(saw_spawn);
  EXPECT_TRUE(saw_dispatch);
  EXPECT_TRUE(saw_completion);
  EXPECT_TRUE(saw_failure);
  EXPECT_EQ(sink.joined().find("diagnostic canary message"), std::string::npos);
}

TEST(StyioObservableRuntime, WaitEpisodesPairReasonsSubjectsAndResolutions) {
  styio_test_setenv("STYIO_TASK_QUEUE_CAPACITY", "2", 1);
  styio_test_setenv("STYIO_TASK_THREADS", "1", 1);
  SessionGuard guard;
  reset_clock();
  CapturingSink sink;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, sink, 1, 64, 16)));
  register_fixture_table();
  StyioObservationDescriptor table[2] = {
    {kFixtureSnapshot, kFixtureTaskSite, 0},
    {kFixtureSnapshot, kFixtureAwaitSite, 1},
  };
  styio_observation_register_table(1, table, 2);

  const int64_t awaited = styio_task_i64_spawn_observed(&task_sleep_then_9, nullptr, 0);
  EXPECT_EQ(styio_task_i64_pull_observed(awaited, 1), 9);
  styio_task_release(awaited);

  const int64_t busy = styio_task_i64_spawn_observed(&task_sleep_then_9, nullptr, 0);
  const int64_t a = styio_task_i64_spawn_observed(&task_return_7, nullptr, 0);
  const int64_t b = styio_task_i64_spawn_observed(&task_return_7, nullptr, 0);
  const int64_t extra = styio_task_i64_spawn_observed(&task_return_7, nullptr, 0);
  EXPECT_EQ(styio_task_i64_pull_observed(busy, 1), 9);
  styio_task_release(busy);
  EXPECT_EQ(styio_task_i64_pull(a), 7);
  EXPECT_EQ(styio_task_i64_pull(b), 7);
  EXPECT_EQ(styio_task_i64_pull(extra), 7);
  styio_task_release(a);
  styio_task_release(b);
  styio_task_release(extra);

  rt::drain();
  rt::end_session();
  const auto records = parse_jsonl(sink.joined());
  std::unordered_set<std::string> open_waits;
  std::unordered_map<std::string, obs::WaitReason> open_reasons;
  bool runnable = false;
  bool task_wait = false;
  bool backpressure = false;
  for (const auto& record : records) {
    if (!record.event.wait.has_value()) {
      continue;
    }
    const auto& wait = *record.event.wait;
    const std::string id = obs::encode_packed_id(obs::kWaitIdPrefix, wait.wait_id);
    if (record.event.kind == obs::EventKind::WaitBegin) {
      open_waits.insert(id);
      open_reasons[id] = wait.reason;
      runnable = runnable || wait.reason == obs::WaitReason::Runnable;
      task_wait = task_wait || wait.reason == obs::WaitReason::Task;
      backpressure = backpressure || wait.reason == obs::WaitReason::Backpressure;
      if (wait.reason == obs::WaitReason::Task) {
        EXPECT_TRUE(wait.subject_present);
      }
    } else if (record.event.kind == obs::EventKind::WaitEnd) {
      EXPECT_NE(open_waits.count(id), 0u) << id;
      open_waits.erase(id);
      open_reasons.erase(id);
      EXPECT_TRUE(
        wait.resolution == obs::WaitResolution::Ready
        || wait.resolution == obs::WaitResolution::Completed
        || wait.resolution == obs::WaitResolution::Failed
        || wait.resolution == obs::WaitResolution::Closed);
    }
  }
  EXPECT_TRUE(runnable);
  EXPECT_TRUE(task_wait);
  EXPECT_TRUE(backpressure);
  for (const auto& id : open_waits) {
    EXPECT_EQ(open_reasons[id], obs::WaitReason::Runnable) << id;
  }
  EXPECT_LE(open_waits.size(), 1u);
}

TEST(StyioObservableRuntime, OpenWaitAtSessionEndMarksSummaryUnresolved) {
  SessionGuard guard;
  reset_clock();
  CapturingSink sink;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, sink)));
  register_fixture_table();
  rt::EmitBuilder begin;
  begin.kind = obs::EventKind::WaitBegin;
  begin.wait_id = rt::next_wait_id();
  begin.waiter = rt::allocate_instance();
  begin.wait_reason = obs::WaitReason::Runnable;
  EXPECT_TRUE(rt::emit_event(begin));
  rt::end_session();
  const std::string payload = sink.joined();
  EXPECT_NE(payload.find("\"event_kind\":\"wait.begin\""), std::string::npos);
  EXPECT_NE(payload.find("\"completeness\":\"partial/unresolved\""), std::string::npos)
    << payload;
}

TEST(StyioObservableRuntime, PairedWaitsKeepSummaryComplete) {
  SessionGuard guard;
  reset_clock();
  CapturingSink sink;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, sink)));
  register_fixture_table();
  const obs::PackedInstance wait_id = rt::next_wait_id();
  const obs::PackedInstance waiter = rt::allocate_instance();
  rt::EmitBuilder begin;
  begin.kind = obs::EventKind::WaitBegin;
  begin.wait_id = wait_id;
  begin.waiter = waiter;
  begin.wait_reason = obs::WaitReason::Runnable;
  EXPECT_TRUE(rt::emit_event(begin));
  rt::EmitBuilder end;
  end.kind = obs::EventKind::WaitEnd;
  end.wait_id = wait_id;
  end.waiter = waiter;
  end.wait_reason = obs::WaitReason::Runnable;
  end.wait_resolution = obs::WaitResolution::Ready;
  EXPECT_TRUE(rt::emit_event(end));
  rt::end_session();
  const std::string payload = sink.joined();
  EXPECT_NE(payload.find("\"completeness\":\"complete\""), std::string::npos) << payload;
}

TEST(StyioObservableRuntime, SecondBeginSessionIsRejectedWhileActive) {
  SessionGuard guard;
  reset_clock();
  CapturingSink sink;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, sink)));
  CapturingSink other_sink;
  EXPECT_FALSE(rt::begin_session(make_config(obs::ObservationMode::Detailed, other_sink)));
  EXPECT_TRUE(rt::session_active());
  rt::end_session();
  EXPECT_FALSE(rt::session_active());
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, other_sink)));
  rt::end_session();
}

TEST(StyioObservableRuntime, UnavailableTransitionsAreCapabilitiesNotSyntheticEvents) {
  SessionGuard guard;
  CapturingSink sink;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, sink)));
  const auto cap = rt::capability_record();
  bool saw_cancel_cap = false;
  bool saw_coop_cap = false;
  for (const auto& name : cap.unavailable_capabilities) {
    saw_cancel_cap = saw_cancel_cap || name == "cancellation";
    saw_coop_cap = saw_coop_cap || name == "cooperative-suspend";
  }
  EXPECT_TRUE(saw_cancel_cap);
  EXPECT_TRUE(saw_coop_cap);
  EXPECT_FALSE(obs::capability_is_supported("cancellation"));
  EXPECT_TRUE(obs::capability_is_unavailable("cancellation"));

  for (int reason = 0; reason <= static_cast<int>(obs::WaitReason::Unknown); ++reason) {
    obs::RuntimeEvent event;
    event.kind = obs::EventKind::WaitBegin;
    event.wait = obs::WaitFields{};
    event.wait->reason = static_cast<obs::WaitReason>(reason);
    event.wait->wait_id = {0, static_cast<std::uint64_t>(reason) + 1};
    event.wait->waiter = {0, 1};
    const std::string json = obs::serialize_runtime_event(event);
    const auto parsed = obs::parse_runtime_record(json);
    ASSERT_TRUE(parsed.ok) << json;
    ASSERT_TRUE(parsed.event.wait.has_value());
    EXPECT_EQ(parsed.event.wait->reason, static_cast<obs::WaitReason>(reason));
    EXPECT_EQ(
      obs::wait_reason_text(parsed.event.wait->reason),
      obs::wait_reason_text(static_cast<obs::WaitReason>(reason)));
  }
  for (int resolution = 0; resolution <= static_cast<int>(obs::WaitResolution::Unknown); ++resolution) {
    obs::RuntimeEvent event;
    event.kind = obs::EventKind::WaitEnd;
    event.wait = obs::WaitFields{};
    event.wait->resolution = static_cast<obs::WaitResolution>(resolution);
    event.wait->wait_id = {0, 1};
    event.wait->waiter = {0, 1};
    const auto parsed = obs::parse_runtime_record(obs::serialize_runtime_event(event));
    ASSERT_TRUE(parsed.ok);
    ASSERT_TRUE(parsed.event.wait.has_value());
    EXPECT_EQ(parsed.event.wait->resolution, static_cast<obs::WaitResolution>(resolution));
  }

  obs::RuntimeEvent cancel;
  cancel.kind = obs::EventKind::CancellationRequested;
  const auto parsed_cancel = obs::parse_runtime_record(obs::serialize_runtime_event(cancel));
  ASSERT_TRUE(parsed_cancel.ok);

  const int64_t handle = styio_task_i64_spawn(&task_return_7, nullptr);
  EXPECT_EQ(styio_task_i64_pull(handle), 7);
  styio_task_release(handle);
  rt::drain();
  rt::end_session();
  const std::string payload = sink.joined();
  EXPECT_EQ(payload.find("cancellation.requested"), std::string::npos);
  EXPECT_EQ(payload.find("cancellation.completed"), std::string::npos);
  EXPECT_EQ(payload.find("cooperative.suspend"), std::string::npos);
  EXPECT_EQ(payload.find("cooperative.resume"), std::string::npos);
}

TEST(StyioObservableRuntime, AggregateModeUsesFixedSiteStorage) {
  SessionGuard guard;
  reset_clock();
  CapturingSink sink;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Aggregate, sink)));
  register_fixture_table();
  for (int i = 0; i < 40; ++i) {
    rt::EmitBuilder pressure;
    pressure.kind = obs::EventKind::QueuePressure;
    pressure.descriptor_index = 0;
    pressure.queue_depth = 3;
    pressure.queue_capacity = 8;
    EXPECT_TRUE(rt::emit_event(pressure));
    rt::EmitBuilder unknown;
    unknown.kind = obs::EventKind::QueuePressure;
    unknown.descriptor_index = obs::kInvalidDescriptorIndex;
    EXPECT_TRUE(rt::emit_event(unknown));
  }
  rt::EmitBuilder created;
  created.kind = obs::EventKind::TaskCreated;
  created.descriptor_index = 0;
  created.instance = rt::allocate_instance();
  EXPECT_TRUE(rt::emit_event(created));
  const auto mid = rt::accounting_snapshot();
  EXPECT_GT(mid.families[static_cast<std::size_t>(obs::EventFamily::Queue)].aggregated, 0u);
  EXPECT_TRUE(obs::accounting_conserved(mid));
  rt::end_session();
  const auto records = parse_jsonl(sink.joined());
  std::size_t shards = 0;
  for (const auto& record : records) {
    if (record.event.kind == obs::EventKind::AggregateShard) {
      ++shards;
    }
    EXPECT_NE(record.event.kind, obs::EventKind::QueuePressure);
  }
  EXPECT_GE(shards, 1u);
  EXPECT_LE(shards, 8u);
}

TEST(StyioObservableRuntime, SampledModeIsDeterministic) {
  auto run_once = []() {
    SessionGuard guard;
    reset_clock();
    CapturingSink sink;
    EXPECT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Sampled, sink)));
    register_fixture_table();
    rt::set_thread_lane(1);
    for (int i = 0; i < 32; ++i) {
      rt::EmitBuilder pressure;
      pressure.kind = obs::EventKind::QueuePressure;
      pressure.descriptor_index = 0;
      pressure.instance = {1, static_cast<std::uint64_t>(i) + 1};
      (void)rt::emit_event(pressure);
    }
    const auto acc = rt::accounting_snapshot();
    rt::drain();
    rt::end_session();
    rt::set_thread_lane(0);
    return std::make_pair(acc, sink.joined());
  };
  const auto first = run_once();
  const auto second = run_once();
  EXPECT_EQ(
    first.first.families[static_cast<std::size_t>(obs::EventFamily::Queue)].sampled_out,
    second.first.families[static_cast<std::size_t>(obs::EventFamily::Queue)].sampled_out);
  EXPECT_EQ(
    first.first.families[static_cast<std::size_t>(obs::EventFamily::Queue)].emitted,
    second.first.families[static_cast<std::size_t>(obs::EventFamily::Queue)].emitted);
  EXPECT_TRUE(obs::accounting_conserved(first.first));
  EXPECT_EQ(first.second, second.second);
  EXPECT_GT(first.first.families[static_cast<std::size_t>(obs::EventFamily::Queue)].sampled_out, 0u);
}

TEST(StyioObservableRuntime, SaturationIsBoundedNonBlockingAndLossAccounted) {
  SessionGuard guard;
  reset_clock();
  CapturingSink sink;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, sink, 2, 64, 32)));
  register_fixture_table();
  const auto start = std::chrono::steady_clock::now();
  std::atomic<int> produced{0};
  auto worker = [&](std::uint32_t lane) {
    rt::set_thread_lane(lane);
    for (int i = 0; i < 200; ++i) {
      rt::EmitBuilder pressure;
      pressure.kind = obs::EventKind::QueuePressure;
      pressure.descriptor_index = 0;
      pressure.queue_depth = 8;
      pressure.queue_capacity = 8;
      (void)rt::emit_event(pressure);
      produced.fetch_add(1);
    }
  };
  std::thread a([&]() { worker(1); });
  std::thread b([&]() { worker(2); });
  a.join();
  b.join();
  rt::set_thread_lane(0);
  rt::EmitBuilder created;
  created.kind = obs::EventKind::TaskCreated;
  created.descriptor_index = 0;
  created.instance = rt::allocate_instance();
  EXPECT_TRUE(rt::emit_event(created));
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_LT(elapsed, std::chrono::seconds(2));
  EXPECT_EQ(produced.load(), 400);
  const auto acc = rt::accounting_snapshot();
  EXPECT_TRUE(obs::accounting_conserved(acc));
  EXPECT_GT(acc.families[static_cast<std::size_t>(obs::EventFamily::Queue)].buffer_dropped, 0u);
  EXPECT_LE(acc.high_water_occupancy, 64u);
  EXPECT_GT(acc.families[static_cast<std::size_t>(obs::EventFamily::TaskLifecycle)].emitted, 0u);
  rt::end_session();
}

TEST(StyioObservableRuntime, ExporterFailureIsIsolated) {
  SessionGuard guard;
  reset_clock();
  CapturingSink sink;
  sink.fail_after = 1;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, sink)));
  register_fixture_table();
  StyioObservationDescriptor table[1] = {{kFixtureSnapshot, kFixtureTaskSite, 0}};
  styio_observation_register_table(1, table, 1);
  const int64_t handle = styio_task_i64_spawn_observed(&task_return_7, nullptr, 0);
  ASSERT_NE(handle, 0);
  EXPECT_EQ(styio_task_i64_pull(handle), 7);
  styio_task_release(handle);
  rt::drain();
  EXPECT_TRUE(rt::exporter_failed());
  const auto acc = rt::accounting_snapshot();
  EXPECT_TRUE(acc.exporter_failed);
  EXPECT_EQ(obs::completeness_from_accounting(acc), obs::Completeness::PartialExporterFailure);
  EXPECT_TRUE(obs::accounting_conserved(acc));
  rt::end_session();
}

TEST(StyioObservableRuntime, DeterministicFixtureIsConsumerIndependent) {
  const std::string text = read_text(fixture_dir() / "canonical.jsonl");
  ASSERT_FALSE(text.empty());
  const auto records = parse_jsonl(text);
  ASSERT_GE(records.size(), 4u);
  bool cap = false;
  bool created = false;
  bool wait_begin = false;
  bool summary = false;
  for (const auto& record : records) {
    ASSERT_TRUE(record.ok) << record.error;
    EXPECT_FALSE(obs::payload_contains_privacy_canary(
      record.record_kind + record.event.snapshot_id + record.event.site_id));
    if (record.record_kind == "session.capability") {
      cap = true;
      EXPECT_EQ(record.capability.schema_version, 2);
      EXPECT_EQ(record.capability.snapshot_id, kFixtureSnapshot);
      EXPECT_EQ(record.capability.mode, obs::ObservationMode::Detailed);
    }
    if (record.event.kind == obs::EventKind::TaskCreated) {
      created = true;
      EXPECT_EQ(record.event.snapshot_id, kFixtureSnapshot);
      EXPECT_EQ(record.event.site_id, kFixtureTaskSite);
      EXPECT_FALSE(record.event.causes.empty());
    }
    if (record.event.kind == obs::EventKind::WaitBegin) {
      wait_begin = true;
      ASSERT_TRUE(record.event.wait.has_value());
      EXPECT_EQ(record.event.wait->reason, obs::WaitReason::Task);
    }
    if (record.record_kind == "session.summary") {
      summary = true;
      EXPECT_EQ(record.summary.execution_id, "x2_0000000000000001");
    }
  }
  EXPECT_TRUE(cap);
  EXPECT_TRUE(created);
  EXPECT_TRUE(wait_begin);
  EXPECT_TRUE(summary);

  obs::RuntimeEvent event;
  event.kind = obs::EventKind::TaskCreated;
  event.correlation = obs::CorrelationStatus::Correlated;
  event.role = obs::SiteRole::Task;
  event.snapshot_id = kFixtureSnapshot;
  event.site_id = kFixtureTaskSite;
  event.instance = {0, 1};
  event.event_ref = {0, 1};
  event.monotonic_ns = 100;
  event.causes.push_back({obs::CausalKind::Spawn, {0, 1}, {0, 1}});
  const auto round_trip = obs::parse_runtime_record(obs::serialize_runtime_event(event));
  ASSERT_TRUE(round_trip.ok);
  EXPECT_EQ(round_trip.event.snapshot_id, kFixtureSnapshot);
  EXPECT_EQ(round_trip.event.site_id, kFixtureTaskSite);
}

TEST(StyioObservableRuntime, UnknownAdditiveFieldsPreserveKnownSemantics) {
  const std::string text = read_text(fixture_dir() / "additive-fields.jsonl");
  ASSERT_FALSE(text.empty());
  for (const auto& line : split_lines(text)) {
    const auto parsed = obs::parse_runtime_record(line);
    ASSERT_TRUE(parsed.ok) << parsed.error << " " << line;
    if (parsed.event.kind == obs::EventKind::TaskCreated) {
      EXPECT_EQ(parsed.event.snapshot_id, kFixtureSnapshot);
      EXPECT_EQ(parsed.event.site_id, kFixtureTaskSite);
      EXPECT_EQ(parsed.event.correlation, obs::CorrelationStatus::Correlated);
    }
  }
  obs::RuntimeEvent event;
  event.kind = obs::EventKind::TaskCompleted;
  event.correlation = obs::CorrelationStatus::Correlated;
  event.snapshot_id = kFixtureSnapshot;
  event.site_id = kFixtureTaskSite;
  event.instance = {0, 2};
  event.event_ref = {0, 4};
  std::string json = obs::serialize_runtime_event(event);
  ASSERT_EQ(json.back(), '}');
  json.pop_back();
  json += ",\"future_hint\":true,\"unknown_enum_sidecar\":\"x\"}";
  const auto parsed = obs::parse_runtime_record(json);
  ASSERT_TRUE(parsed.ok) << parsed.error;
  EXPECT_EQ(parsed.event.kind, obs::EventKind::TaskCompleted);
  EXPECT_EQ(parsed.event.snapshot_id, kFixtureSnapshot);
  EXPECT_EQ(parsed.event.site_id, kFixtureTaskSite);
}

TEST(StyioObservableRuntime, StrictPrivacyFixtureExcludesCanaries) {
  const auto canaries = obs::privacy_canaries();
  ASSERT_FALSE(canaries.empty());
  for (const auto name : {"canonical.jsonl", "additive-fields.jsonl"}) {
    const std::string text = read_text(fixture_dir() / name);
    EXPECT_FALSE(obs::payload_contains_privacy_canary(text)) << name;
  }
  const std::string source = read_text(fixture_dir() / "privacy-canaries.styio");
  EXPECT_TRUE(obs::payload_contains_privacy_canary(source));

  SessionGuard guard;
  CapturingSink sink;
  ASSERT_TRUE(rt::begin_session(make_config(obs::ObservationMode::Detailed, sink)));
  register_fixture_table();
  const int64_t handle = styio_task_i64_spawn_observed(&task_throw_canary, nullptr, 0);
  (void)styio_task_i64_pull(handle);
  styio_task_release(handle);
  rt::drain();
  rt::end_session();
  const std::string payload = sink.joined();
  EXPECT_FALSE(obs::payload_contains_privacy_canary(payload)) << payload;
  EXPECT_EQ(payload.find("/absolute/canary/path"), std::string::npos);
  EXPECT_EQ(payload.find("CANARY_SECRET_sk-live-test"), std::string::npos);
  EXPECT_EQ(payload.find("diagnostic canary message"), std::string::npos);
}
