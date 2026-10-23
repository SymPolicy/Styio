#include <gtest/gtest.h>

#include "observable_topology_fixtures.hpp"

namespace {

void expect_equivalent(const obs::QueryResponse& lhs, const obs::QueryResponse& rhs) {
  EXPECT_EQ(lhs.status, rhs.status);
  EXPECT_EQ(lhs.completeness, rhs.completeness);
  EXPECT_EQ(lhs.truncated, rhs.truncated);
  EXPECT_EQ(lhs.visited, rhs.visited);
  EXPECT_EQ(lhs.snapshot_id, rhs.snapshot_id);
  ASSERT_EQ(lhs.results.size(), rhs.results.size());
  for (std::size_t i = 0; i < lhs.results.size(); ++i) {
    EXPECT_EQ(lhs.results[i].category, rhs.results[i].category);
    EXPECT_EQ(lhs.results[i].id, rhs.results[i].id);
    EXPECT_EQ(lhs.results[i].json, rhs.results[i].json);
  }
  EXPECT_EQ(lhs.evidence, rhs.evidence);
}

obs::QueryRequest make_request(obs::QueryKind kind, std::string subject, std::string target = {}) {
  obs::QueryRequest request;
  request.kind = kind;
  request.subject = std::move(subject);
  request.target = std::move(target);
  return request;
}

} // namespace

TEST(StyioObservableQuery, IndexMatchesReferenceForEveryQueryKind) {
  using observable_fixtures::base_parent;
  using observable_fixtures::kProgram;
  using observable_fixtures::kHandle;
  using observable_fixtures::kSink;
  using observable_fixtures::kTask;
  using observable_fixtures::kStream;
  const auto snapshot = base_parent();
  const auto index = obs::build_snapshot_index(snapshot);
  const std::vector<obs::QueryRequest> requests{
    make_request(obs::QueryKind::Lookup, kProgram),
    make_request(obs::QueryKind::Dependencies, kProgram),
    make_request(obs::QueryKind::Dependents, kSink),
    make_request(obs::QueryKind::Effects, kProgram),
    make_request(obs::QueryKind::Ownership, kProgram),
    make_request(obs::QueryKind::Mutation, kHandle),
    make_request(obs::QueryKind::Failure, kTask),
    make_request(obs::QueryKind::TaskScope, kTask),
    make_request(obs::QueryKind::StreamScope, kStream),
    make_request(obs::QueryKind::Impact, kSink),
    make_request(obs::QueryKind::CanonicalPath, kProgram, kSink),
    make_request(obs::QueryKind::Lineage, kHandle),
    make_request(obs::QueryKind::Why, kProgram),
  };
  for (const auto& request : requests) {
    const auto reference = obs::evaluate_query_reference(snapshot, request);
    obs::QueryIndexProbe probe;
    const auto indexed = obs::evaluate_query_index(snapshot, index, request, &probe);
    expect_equivalent(reference, indexed);
    // The indexed path must actually consult shards, not call through to the
    // reference evaluator.
    EXPECT_GT(probe.shard_lookups, 0u) << obs::query_kind_name(request.kind);
    EXPECT_LE(reference.visited, request.limits.max_visited == 0 ? obs::kQueryDefaultVisited : request.limits.max_visited);
    EXPECT_LE(reference.evidence_count, obs::kQueryDefaultEvidence);
  }
}

TEST(StyioObservableQuery, MergedIndexServesChildQueries) {
  using observable_fixtures::base_parent;
  using observable_fixtures::child_add_diagnostic;
  using observable_fixtures::child_field_change;
  using observable_fixtures::child_remove_tie_b;
  using observable_fixtures::kProgram;
  using observable_fixtures::kHandle;
  using observable_fixtures::kSink;
  const auto parent = base_parent();
  const auto parent_index = obs::build_snapshot_index(parent);

  const std::vector<std::pair<std::string, obs::Snapshot>> children{
    {"field-change", child_field_change()},
    {"add-diagnostic", child_add_diagnostic()},
    {"remove", child_remove_tie_b()},
  };
  for (const auto& [name, child] : children) {
    const auto delta = obs::generate_delta(parent, child);
    ASSERT_TRUE(delta.ok) << name << ": " << delta.error;
    const auto merged = obs::merge_snapshot_index(parent_index, child, delta.delta);
    EXPECT_EQ(merged.snapshot_id, obs::snapshot_identity(child)) << name;
    EXPECT_GT(merged.reused_shards, 0u) << name;
    const std::vector<obs::QueryRequest> requests{
      make_request(obs::QueryKind::Lookup, kProgram),
      make_request(obs::QueryKind::Dependencies, kProgram),
      make_request(obs::QueryKind::Dependents, kSink),
      make_request(obs::QueryKind::Effects, kProgram),
      make_request(obs::QueryKind::Ownership, kProgram),
      make_request(obs::QueryKind::Mutation, kHandle),
      make_request(obs::QueryKind::TaskScope, observable_fixtures::kTask),
      make_request(obs::QueryKind::StreamScope, observable_fixtures::kStream),
      make_request(obs::QueryKind::Impact, kSink),
      make_request(obs::QueryKind::CanonicalPath, kProgram, kSink),
      make_request(obs::QueryKind::Lineage, kHandle),
      make_request(obs::QueryKind::Why, kProgram),
    };
    for (const auto& request : requests) {
      const auto reference = obs::evaluate_query_reference(child, request);
      obs::QueryIndexProbe probe;
      const auto indexed = obs::evaluate_query_index(child, merged, request, &probe);
      expect_equivalent(reference, indexed);
      EXPECT_GT(probe.shard_lookups, 0u) << name << ": " << obs::query_kind_name(request.kind);
    }
  }

  // The removal child must drop the removed tie edge from the merged index:
  // dependencies of the program no longer list the removed Flow edge.
  const auto child = child_remove_tie_b();
  const auto delta = obs::generate_delta(parent, child);
  ASSERT_TRUE(delta.ok);
  const auto merged = obs::merge_snapshot_index(parent_index, child, delta.delta);
  const auto dependencies = obs::evaluate_query_index(
    child, merged, make_request(obs::QueryKind::Dependencies, kProgram));
  EXPECT_EQ(dependencies.status, obs::QueryStatus::Complete);
  for (const auto& record : dependencies.results) {
    EXPECT_NE(record.id, observable_fixtures::kEFlowB);
  }
  EXPECT_EQ(dependencies.results.size(), 3u);
}

TEST(StyioObservableQuery, CanonicalPathAndWhyAreBounded) {
  using observable_fixtures::base_parent;
  using observable_fixtures::kProgram;
  using observable_fixtures::kSink;
  using observable_fixtures::kFProgramState;
  const auto snapshot = base_parent();
  const auto index = obs::build_snapshot_index(snapshot);

  auto path_request = make_request(obs::QueryKind::CanonicalPath, kProgram, kSink);
  const auto path = obs::evaluate_query_reference(snapshot, path_request);
  const auto path_index = obs::evaluate_query_index(snapshot, index, path_request);
  expect_equivalent(path, path_index);
  ASSERT_GE(path.results.size(), 3u);
  EXPECT_EQ(path.results.front().id, kProgram);
  EXPECT_EQ(path.results[1].id, observable_fixtures::kTieA);
  EXPECT_EQ(path.results.back().id, kSink);
  EXPECT_EQ(path.status, obs::QueryStatus::Complete);

  path_request.limits.max_depth = 1;
  path_request.limits.max_visited = 4;
  const auto truncated = obs::evaluate_query_reference(snapshot, path_request);
  EXPECT_TRUE(truncated.truncated || truncated.status == obs::QueryStatus::Truncated
              || truncated.results.empty());
  EXPECT_LE(truncated.visited, 4u);

  auto why_request = make_request(obs::QueryKind::Why, kFProgramState);
  why_request.limits.max_evidence = 1;
  const auto why = obs::evaluate_query_reference(snapshot, why_request);
  EXPECT_LE(why.evidence_count, 1u);
  EXPECT_TRUE(why.truncated || why.status == obs::QueryStatus::Truncated || why.evidence_count <= 1);

  auto hard = make_request(obs::QueryKind::Impact, kSink);
  hard.limits.max_results = obs::kQueryHardResults + 10;
  hard.limits.max_visited = obs::kQueryHardVisited + 10;
  const auto capped = obs::evaluate_query_reference(snapshot, hard);
  EXPECT_LE(capped.visited, obs::kQueryHardVisited);
  EXPECT_LE(capped.results.size(), obs::kQueryHardResults);
}

TEST(StyioObservableQuery, CompletenessControlsNegativeAnswers) {
  using observable_fixtures::base_parent;
  using observable_fixtures::partial_snapshot;
  const auto complete = base_parent();
  const auto partial = partial_snapshot();
  auto missing = make_request(obs::QueryKind::Lookup, "n1_ffffffffffffffffffffffffffffff00");
  const auto complete_answer = obs::evaluate_query_reference(complete, missing);
  const auto partial_answer = obs::evaluate_query_reference(partial, missing);
  EXPECT_EQ(complete_answer.status, obs::QueryStatus::Complete);
  EXPECT_TRUE(complete_answer.results.empty());
  EXPECT_EQ(partial_answer.status, obs::QueryStatus::Partial);
  EXPECT_TRUE(partial_answer.results.empty());
  EXPECT_EQ(partial_answer.reason, "partial");

  const auto empty_complete = obs::evaluate_query_reference(
    complete, make_request(obs::QueryKind::Lineage, observable_fixtures::kHandle));
  EXPECT_EQ(empty_complete.status, obs::QueryStatus::Complete);
  EXPECT_TRUE(empty_complete.results.empty());
}
