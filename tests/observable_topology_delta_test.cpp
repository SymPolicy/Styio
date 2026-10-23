#include <gtest/gtest.h>

#include "observable_topology_fixtures.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif

namespace fs = std::filesystem;
using namespace observable_fixtures;

namespace {

std::string read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

fs::path fixture_root() {
  return fs::path(STYIO_SOURCE_DIR) / "tests/fixtures/observable-topology";
}

obs::Snapshot must_finalize(obs::Snapshot snapshot) {
  auto finished = obs::finalize_snapshot(std::move(snapshot));
  EXPECT_TRUE(finished.ok) << finished.error;
  return finished.snapshot;
}

} // namespace

TEST(StyioObservableDelta, CanonicalGenerationIsDeterministic) {
  const auto parent = base_parent();
  const auto child = child_field_change();
  const auto first = obs::generate_delta(parent, child);
  const auto second = obs::generate_delta(parent, child);
  ASSERT_TRUE(first.ok) << first.error;
  ASSERT_TRUE(second.ok) << second.error;
  EXPECT_EQ(first.json, second.json);
  EXPECT_FALSE(first.delta.operations.empty());
}

TEST(StyioObservableDelta, ApplyReconstructsEveryFixtureExactly) {
  const std::vector<std::pair<obs::Snapshot, obs::Snapshot>> pairs{
    {base_parent(), base_parent()},
    {base_parent(), child_field_change()},
    {base_parent(), child_add_diagnostic()},
    {base_parent(), child_remove_tie_b()},
    {base_parent(), child_unrelated_sink_state()},
    {base_parent(), child_rename_handle()},
    {base_parent(), child_split_stream()},
  };
  for (const auto& [parent, child] : pairs) {
    const auto parent_bytes = obs::serialize_snapshot(parent);
    const auto generated = obs::generate_delta(parent, child);
    ASSERT_TRUE(generated.ok) << generated.error;
    const auto applied = obs::apply_delta(parent, generated.delta);
    ASSERT_TRUE(applied.ok) << applied.error;
    EXPECT_EQ(obs::serialize_snapshot(applied.snapshot), obs::serialize_snapshot(child));
    EXPECT_EQ(obs::serialize_snapshot(parent), parent_bytes);
    EXPECT_EQ(applied.snapshot_id, obs::snapshot_identity(child));
  }

  const auto parent = base_parent();
  auto generated = obs::generate_delta(parent, child_field_change());
  ASSERT_TRUE(generated.ok);
  generated.delta.parent_snapshot_id = "s1_deadbeefdeadbeefdeadbeefdeadbeef";
  const auto wrong_base = obs::apply_delta(parent, generated.delta);
  EXPECT_FALSE(wrong_base.ok);
  EXPECT_EQ(wrong_base.reason, obs::kReasonWrongBase);

  generated = obs::generate_delta(parent, child_field_change());
  ASSERT_TRUE(generated.ok);
  generated.delta.operations.push_back(obs::DeltaOperation{
    obs::DeltaOpKind::Remove, obs::RecordCategory::Nodes, "n1_missingmissingmissingmissingmi", {}, {}});
  const auto malformed = obs::apply_delta(parent, generated.delta);
  EXPECT_FALSE(malformed.ok);
}

TEST(StyioObservableDelta, CoversAllSnapshotRecordCategoriesAndFieldChanges) {
  const auto parent = base_parent();
  auto child = child_field_change();
  child = child_add_diagnostic();
  for (auto& fact : child.facts) {
    if (fact.id == kFHandleState) {
      fact.string_value = "Open";
    }
  }
  child.producer.version = "0.0.2";
  child = must_finalize(child);
  const auto generated = obs::generate_delta(parent, child);
  ASSERT_TRUE(generated.ok) << generated.error;
  bool saw_metadata = false;
  bool saw_fact = false;
  bool saw_diagnostic = false;
  for (const auto& op : generated.delta.operations) {
    if (op.category == obs::RecordCategory::Metadata && op.kind == obs::DeltaOpKind::ReplaceFields) {
      saw_metadata = true;
    }
    if (op.category == obs::RecordCategory::Facts && op.kind == obs::DeltaOpKind::ReplaceFields) {
      saw_fact = true;
      ASSERT_FALSE(op.fields.empty());
      EXPECT_EQ(op.fields.front().name, "value");
    }
    if (op.category == obs::RecordCategory::Diagnostics && op.kind == obs::DeltaOpKind::Add) {
      saw_diagnostic = true;
    }
  }
  EXPECT_TRUE(saw_metadata);
  EXPECT_TRUE(saw_fact);
  EXPECT_TRUE(saw_diagnostic);

  const auto removed = obs::generate_delta(parent, child_remove_tie_b());
  ASSERT_TRUE(removed.ok);
  bool saw_node_remove = false;
  bool saw_edge_remove = false;
  bool saw_evidence_remove = false;
  for (const auto& op : removed.delta.operations) {
    if (op.category == obs::RecordCategory::Nodes && op.kind == obs::DeltaOpKind::Remove) {
      saw_node_remove = true;
    }
    if (op.category == obs::RecordCategory::Edges && op.kind == obs::DeltaOpKind::Remove) {
      saw_edge_remove = true;
    }
    if (op.category == obs::RecordCategory::Evidence && op.kind == obs::DeltaOpKind::Remove) {
      saw_evidence_remove = true;
    }
  }
  EXPECT_TRUE(saw_node_remove);
  EXPECT_TRUE(saw_edge_remove);
  EXPECT_TRUE(saw_evidence_remove);

  const auto added = obs::generate_delta(parent, child_split_stream());
  ASSERT_TRUE(added.ok);
  bool saw_node_add = false;
  for (const auto& op : added.delta.operations) {
    if (op.category == obs::RecordCategory::Nodes && op.kind == obs::DeltaOpKind::Add) {
      saw_node_add = true;
    }
  }
  EXPECT_TRUE(saw_node_add);
}

TEST(StyioObservableLineage, AcceptsOnlyProducerEvidence) {
  const auto parent = base_parent();
  auto child = child_rename_handle();
  obs::LineageDraft draft;
  draft.kind = "rename";
  draft.prior = {kHandle};
  draft.target = {kHandleRenamed};
  draft.producer_rule = "styio.sema.topology.lineage.rename";
  draft.rule_version = "1";
  draft.evidence = {kEvHandleRenamed};
  const auto accepted = obs::construct_lineage(parent, child, {draft});
  ASSERT_TRUE(accepted.ok) << accepted.error;
  ASSERT_EQ(accepted.records.size(), 1u);
  EXPECT_EQ(accepted.records[0].kind, "rename");

  obs::LineageDraft hint = draft;
  hint.producer_rule = "consumer.suggested.rename";
  const auto rejected = obs::construct_lineage(parent, child, {hint});
  EXPECT_FALSE(rejected.ok);

  child.lineage = accepted.records;
  child = must_finalize(child);
  const auto generated = obs::generate_delta(parent, child);
  ASSERT_TRUE(generated.ok);
  const auto parsed = obs::parse_delta(generated.json);
  ASSERT_TRUE(parsed.ok);
  const auto applied = obs::apply_delta(parent, parsed.delta);
  ASSERT_TRUE(applied.ok);
  ASSERT_EQ(applied.snapshot.lineage.size(), 1u);
}

TEST(StyioObservableLineage, EnforcesRelationCardinalityAndMembership) {
  const auto parent = base_parent();
  auto split_child = child_split_stream();
  obs::LineageDraft split;
  split.kind = "split";
  split.prior = {kStream};
  split.target = {kSplitA, kSplitB};
  split.producer_rule = "styio.sema.topology.lineage.split";
  split.rule_version = "1";
  split.evidence = {kEvStream};
  ASSERT_TRUE(obs::construct_lineage(parent, split_child, {split}).ok);

  split.target = {kSplitA};
  EXPECT_FALSE(obs::construct_lineage(parent, split_child, {split}).ok);

  obs::LineageDraft merge;
  merge.kind = "merge";
  merge.prior = {kTieA, kTieB};
  merge.target = {kTieA};
  merge.producer_rule = "styio.sema.topology.lineage.merge";
  merge.rule_version = "1";
  merge.evidence = {kEvTieA};
  auto merge_child = child_remove_tie_b();
  ASSERT_TRUE(obs::construct_lineage(parent, merge_child, {merge}).ok);
  merge.prior = {kTieA};
  EXPECT_FALSE(obs::construct_lineage(parent, merge_child, {merge}).ok);

  obs::LineageDraft move;
  move.kind = "move";
  move.prior = {kHandle};
  move.target = {kHandleRenamed};
  move.producer_rule = "styio.sema.topology.lineage.move";
  move.rule_version = "1";
  move.evidence = {kEvHandleRenamed};
  ASSERT_TRUE(obs::construct_lineage(parent, child_rename_handle(), {move}).ok);

  obs::LineageDraft missing_subject = move;
  missing_subject.prior = {"n1_ffffffffffffffffffffffffffffff00"};
  EXPECT_FALSE(obs::construct_lineage(parent, child_rename_handle(), {missing_subject}).ok);

  obs::LineageDraft unresolved = move;
  unresolved.evidence = {"v1_ffffffffffffffffffffffffffffff00"};
  EXPECT_FALSE(obs::construct_lineage(parent, child_rename_handle(), {unresolved}).ok);
}

TEST(StyioObservableLineage, AbsenceAndHintsNeverInferRelations) {
  const auto parent = base_parent();
  const auto child = child_rename_handle();
  const auto empty = obs::construct_lineage(parent, child, {});
  ASSERT_TRUE(empty.ok);
  EXPECT_TRUE(empty.records.empty());

  obs::LineageDraft label_hint;
  label_hint.kind = "rename";
  label_hint.prior = {"Handle"};
  label_hint.target = {"ResourceHandle"};
  label_hint.producer_rule = "styio.sema.topology.lineage.rename";
  label_hint.rule_version = "1";
  label_hint.evidence = {kEvHandle};
  EXPECT_FALSE(obs::construct_lineage(parent, child, {label_hint}).ok);

  const auto generated = obs::generate_delta(parent, child);
  ASSERT_TRUE(generated.ok);
  bool inferred_lineage = false;
  for (const auto& op : generated.delta.operations) {
    if (op.category == obs::RecordCategory::Lineage) {
      inferred_lineage = true;
    }
  }
  EXPECT_FALSE(inferred_lineage);
}

TEST(StyioObservableDelta, ApplyReconstructsChildNamingParent) {
  const auto parent = base_parent();
  auto child = child_field_change();
  child.parent_snapshot_id = obs::snapshot_identity(parent);
  child = must_finalize(std::move(child));
  const auto generated = obs::generate_delta(parent, child);
  ASSERT_TRUE(generated.ok) << generated.error;
  bool saw_parent_field = false;
  for (const auto& op : generated.delta.operations) {
    if (op.category != obs::RecordCategory::Metadata) {
      continue;
    }
    for (const auto& field : op.fields) {
      if (field.name == "parent_snapshot_id") {
        saw_parent_field = true;
        EXPECT_EQ(field.before, "null");
      }
    }
  }
  EXPECT_TRUE(saw_parent_field);
  const auto applied = obs::apply_delta(parent, generated.delta);
  ASSERT_TRUE(applied.ok) << applied.error;
  EXPECT_EQ(applied.snapshot_id, obs::snapshot_identity(child));
  EXPECT_EQ(obs::serialize_snapshot(applied.snapshot), obs::serialize_snapshot(child));
}

TEST(StyioObservableDelta, ApplyRejectsIdentityFieldReplacement) {
  const auto parent = base_parent();
  const auto parent_id = obs::snapshot_identity(parent);
  auto identity_replace = [&](obs::RecordCategory category, std::string key, std::string field) {
    obs::TopologyDelta delta;
    delta.parent_snapshot_id = parent_id;
    delta.target_snapshot_id = "s1_ffffffffffffffffffffffffffffffff";
    delta.operations.push_back(obs::DeltaOperation{
      obs::DeltaOpKind::ReplaceFields,
      category,
      std::move(key),
      {},
      {obs::FieldReplacement{std::move(field), "null", "null"}},
    });
    return obs::apply_delta(parent, delta);
  };
  EXPECT_EQ(identity_replace(obs::RecordCategory::Edges, kEOwn, "from").reason,
            obs::kReasonMalformedOperation);
  EXPECT_EQ(identity_replace(obs::RecordCategory::Edges, kEOwn, "to").reason,
            obs::kReasonMalformedOperation);
  EXPECT_EQ(identity_replace(obs::RecordCategory::Edges, kEOwn, "kind").reason,
            obs::kReasonMalformedOperation);
  EXPECT_EQ(identity_replace(obs::RecordCategory::Facts, kFHandleState, "subject").reason,
            obs::kReasonMalformedOperation);
  EXPECT_EQ(identity_replace(obs::RecordCategory::Facts, kFHandleState, "predicate").reason,
            obs::kReasonMalformedOperation);
  EXPECT_EQ(identity_replace(obs::RecordCategory::Anchors, kAnchor, "path").reason,
            obs::kReasonMalformedOperation);
  EXPECT_EQ(identity_replace(obs::RecordCategory::Evidence, kEvHandle, "subjects").reason,
            obs::kReasonMalformedOperation);
  EXPECT_EQ(identity_replace(obs::RecordCategory::Diagnostics, kDiag, "subject").reason,
            obs::kReasonMalformedOperation);
  EXPECT_EQ(identity_replace(obs::RecordCategory::Lineage, "l1_ffffffffffffffffffffffffffffffff", "kind").reason,
            obs::kReasonMalformedOperation);

  // Non-key node fields remain replaceable end to end.
  auto kind_child = base_parent();
  for (auto& node : kind_child.nodes) {
    if (node.id == kTieA) {
      node.kind = "ChangedKind";
    }
  }
  kind_child = must_finalize(std::move(kind_child));
  const auto generated = obs::generate_delta(parent, kind_child);
  ASSERT_TRUE(generated.ok) << generated.error;
  bool saw_node_kind = false;
  for (const auto& op : generated.delta.operations) {
    if (op.category == obs::RecordCategory::Nodes && op.kind == obs::DeltaOpKind::ReplaceFields) {
      saw_node_kind = true;
      EXPECT_EQ(op.fields.front().name, "kind");
    }
  }
  EXPECT_TRUE(saw_node_kind);
  const auto applied = obs::apply_delta(parent, generated.delta);
  ASSERT_TRUE(applied.ok) << applied.error;
  EXPECT_EQ(applied.snapshot_id, obs::snapshot_identity(kind_child));
}

TEST(StyioObservableDelta, ApplyRequiresCanonicalOperationOrder) {
  const auto parent = base_parent();
  auto child = child_field_change();
  child.producer.version = "0.0.2";
  child = must_finalize(std::move(child));
  const auto generated = obs::generate_delta(parent, child);
  ASSERT_TRUE(generated.ok) << generated.error;
  ASSERT_GE(generated.delta.operations.size(), 2u);

  auto reordered = generated.delta;
  std::swap(reordered.operations[0], reordered.operations[1]);
  const auto unsorted = obs::apply_delta(parent, reordered);
  EXPECT_FALSE(unsorted.ok);
  EXPECT_EQ(unsorted.reason, obs::kReasonMalformedOperation);

  auto duplicated = generated.delta;
  duplicated.operations.push_back(duplicated.operations.front());
  const auto dup = obs::apply_delta(parent, duplicated);
  EXPECT_FALSE(dup.ok);
  EXPECT_EQ(dup.reason, obs::kReasonMalformedOperation);
}

TEST(StyioObservableDelta, CheckedFixturesRoundTrip) {
  const auto parent_json = read_text(fixture_root() / "parent" / "complete.json");
  const auto child_json = read_text(fixture_root() / "child" / "field-change.json");
  const auto parent = obs::parse_snapshot(parent_json);
  const auto child = obs::parse_snapshot(child_json);
  ASSERT_TRUE(parent.ok) << parent.error;
  ASSERT_TRUE(child.ok) << child.error;
  const auto generated = obs::generate_delta(parent.snapshot, child.snapshot);
  ASSERT_TRUE(generated.ok) << generated.error;
  const auto applied = obs::apply_delta(parent.snapshot, generated.delta);
  ASSERT_TRUE(applied.ok) << applied.error;
  EXPECT_EQ(applied.snapshot_id, child.snapshot_id);
}

TEST(StyioObservableDelta, CheckedFixturesCoverEveryManifestFamily) {
  const auto parent_text = read_text(fixture_root() / "parent" / "complete.json");
  const auto parent = obs::parse_snapshot(parent_text);
  ASSERT_TRUE(parent.ok) << parent.error;
  const std::vector<std::pair<std::string, std::string>> families{
    {"unchanged", "unchanged"},
    {"field-change", "field-change"},
    {"add-diagnostic", "add"},
    {"remove", "remove"},
    {"rename", "rename"},
    {"move", "move"},
    {"split", "split"},
    {"merge", "merge"},
  };
  for (const auto& [child_name, delta_name] : families) {
    const auto child_text = read_text(fixture_root() / "child" / (child_name + ".json"));
    const auto delta_text = read_text(fixture_root() / "delta" / (delta_name + ".json"));
    const auto child = obs::parse_snapshot(child_text);
    const auto delta = obs::parse_delta(delta_text);
    ASSERT_TRUE(child.ok) << child_name << ": " << child.error;
    ASSERT_TRUE(delta.ok) << delta_name << ": " << delta.error;
    EXPECT_EQ(delta.delta.parent_snapshot_id, parent.snapshot_id) << delta_name;
    EXPECT_EQ(delta.delta.target_snapshot_id, child.snapshot_id) << delta_name;
    const auto applied = obs::apply_delta(parent.snapshot, delta.delta);
    ASSERT_TRUE(applied.ok) << delta_name << ": " << applied.error;
    EXPECT_EQ(applied.snapshot_id, child.snapshot_id) << delta_name;
    EXPECT_EQ(obs::serialize_snapshot(applied.snapshot), child_text) << child_name;
  }

  const auto malformed = obs::parse_delta(read_text(fixture_root() / "delta" / "malformed.json"));
  ASSERT_TRUE(malformed.ok) << malformed.error;
  const auto malformed_applied = obs::apply_delta(parent.snapshot, malformed.delta);
  EXPECT_FALSE(malformed_applied.ok);
  EXPECT_EQ(malformed_applied.reason, obs::kReasonMalformedOperation);

  const auto unsupported = obs::parse_delta(read_text(fixture_root() / "delta" / "unsupported.json"));
  ASSERT_TRUE(unsupported.ok) << unsupported.error;
  const auto unsupported_applied = obs::apply_delta(parent.snapshot, unsupported.delta);
  EXPECT_FALSE(unsupported_applied.ok);
  EXPECT_EQ(unsupported_applied.reason, obs::kReasonUnsupported);
}
