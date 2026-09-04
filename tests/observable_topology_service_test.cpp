#include <gtest/gtest.h>

#include "observable_topology_fixtures.hpp"

TEST(StyioObservableService, InvalidatesOnlyDependentIndexShards) {
  using namespace observable_fixtures;
  obs::ObservableTopologyService service(obs::qualified_scope_key(base_parent()));
  ASSERT_TRUE(service.publish(base_parent()).ok);
  const auto* handle_before = service.shard(kHandle);
  const auto* sink_before = service.shard(kSink);
  ASSERT_NE(handle_before, nullptr);
  ASSERT_NE(sink_before, nullptr);
  const auto* handle_ptr = handle_before;
  const auto* sink_ptr = sink_before;

  const auto published = service.publish(child_unrelated_sink_state());
  ASSERT_TRUE(published.ok) << published.error;
  const auto* handle_after = service.shard(kHandle);
  const auto* sink_after = service.shard(kSink);
  ASSERT_NE(handle_after, nullptr);
  ASSERT_NE(sink_after, nullptr);
  EXPECT_EQ(handle_after, handle_ptr);
  EXPECT_NE(sink_after, sink_ptr);
  EXPECT_GE(service.counters().reused_shards, 1u);
  EXPECT_GE(service.counters().rebuilt_shards, 1u);
}

TEST(StyioObservableService, RetentionAndEvictionStayBounded) {
  using namespace observable_fixtures;
  obs::RetentionLimits limits;
  limits.max_snapshots = 2;
  limits.max_deltas = 1;
  limits.max_snapshot_bytes = 256 * 1024;
  limits.max_index_bytes = 256 * 1024;
  obs::ObservableTopologyService service(obs::qualified_scope_key(base_parent()), limits);
  ASSERT_TRUE(service.publish(base_parent()).ok);
  ASSERT_TRUE(service.publish(child_field_change()).ok);
  EXPECT_TRUE(service.parent().has_value());
  EXPECT_TRUE(service.delta().has_value());
  EXPECT_LE(service.counters().retained_snapshot_bytes, limits.max_snapshot_bytes);
  EXPECT_LE(service.counters().retained_index_bytes, limits.max_index_bytes);

  obs::RetentionLimits tiny;
  tiny.max_snapshot_bytes = 16;
  tiny.max_index_bytes = 16;
  obs::ObservableTopologyService tight(obs::qualified_scope_key(base_parent()), tiny);
  const auto oversized = tight.publish(base_parent());
  EXPECT_FALSE(oversized.ok);
  EXPECT_EQ(oversized.reason, obs::kReasonResourceLimit);
  EXPECT_FALSE(tight.current().has_value());
}

TEST(StyioObservableService, CacheLossMatchesReference) {
  using namespace observable_fixtures;
  obs::ObservableTopologyService service(obs::qualified_scope_key(base_parent()));
  ASSERT_TRUE(service.publish(base_parent()).ok);
  auto request = obs::QueryRequest{};
  request.kind = obs::QueryKind::CanonicalPath;
  request.subject = kProgram;
  request.target = kSink;
  const auto warm = service.query(request);
  // A retained index serves the query; the reference evaluator is not needed.
  EXPECT_EQ(service.counters().reference_fallbacks, 0u);
  service.drop_derived_indexes();
  EXPECT_FALSE(service.index_present());
  const auto cold = service.query(request);
  EXPECT_EQ(warm.status, cold.status);
  ASSERT_EQ(warm.results.size(), cold.results.size());
  for (std::size_t i = 0; i < warm.results.size(); ++i) {
    EXPECT_EQ(warm.results[i].id, cold.results[i].id);
  }
  EXPECT_EQ(warm.visited, cold.visited);
  EXPECT_GE(service.counters().reference_fallbacks, 1u);
}

TEST(StyioObservableService, SingleSnapshotRetentionDropsParentAndDelta) {
  using namespace observable_fixtures;
  obs::RetentionLimits limits;
  limits.max_snapshots = 1;
  obs::ObservableTopologyService service(obs::qualified_scope_key(base_parent()), limits);
  ASSERT_TRUE(service.publish(base_parent()).ok);
  ASSERT_TRUE(service.publish(child_field_change()).ok);
  EXPECT_TRUE(service.current().has_value());
  EXPECT_FALSE(service.parent().has_value());
  EXPECT_FALSE(service.delta().has_value());
  const auto parent_query = service.query_parent(obs::QueryRequest{});
  EXPECT_EQ(parent_query.reason, obs::kReasonFullSnapshotRequired);
  EXPECT_LE(service.counters().retained_snapshot_bytes, limits.max_snapshot_bytes);
}

TEST(StyioObservableService, InvalidPublicationPreservesPriorState) {
  using namespace observable_fixtures;
  obs::ObservableTopologyService service(obs::qualified_scope_key(base_parent()));
  const auto first = service.publish(base_parent());
  ASSERT_TRUE(first.ok);
  const auto prior_id = first.snapshot_id;
  const auto prior_json = obs::serialize_snapshot(*service.current());

  auto invalid = base_parent();
  invalid.edges.front().to = "n1_ffffffffffffffffffffffffffffff00";
  const auto failed = service.publish(invalid);
  EXPECT_FALSE(failed.ok);
  ASSERT_TRUE(service.current().has_value());
  EXPECT_EQ(obs::snapshot_identity(*service.current()), prior_id);
  EXPECT_EQ(obs::serialize_snapshot(*service.current()), prior_json);

  const auto parent_query = service.query_parent(obs::QueryRequest{});
  EXPECT_EQ(parent_query.reason, obs::kReasonFullSnapshotRequired);

  ASSERT_TRUE(service.publish(child_field_change()).ok);
  auto parent_lookup = obs::QueryRequest{};
  parent_lookup.kind = obs::QueryKind::Lookup;
  parent_lookup.subject = kProgram;
  const auto after_evict = service.query_parent(parent_lookup);
  EXPECT_NE(after_evict.reason, obs::kReasonFullSnapshotRequired);
  EXPECT_EQ(after_evict.status, obs::QueryStatus::Complete);
  // The retained parent index serves parent queries without a reference
  // fallback.
  EXPECT_EQ(service.counters().reference_fallbacks, 0u);
}
