#pragma once

#include "StyioServices/StyioObservable/Delta.hpp"
#include "StyioServices/StyioObservable/Query.hpp"
#include "StyioServices/StyioObservable/Service.hpp"

#include <algorithm>
#include <string>

namespace obs = styio::observable;

namespace observable_fixtures {

inline constexpr const char* kProgram = "n1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa0";
inline constexpr const char* kHandle = "n1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1";
inline constexpr const char* kStream = "n1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2";
inline constexpr const char* kTask = "n1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa3";
inline constexpr const char* kSink = "n1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa4";
inline constexpr const char* kTieA = "n1_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1";
inline constexpr const char* kTieB = "n1_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2";
inline constexpr const char* kHandleRenamed = "n1_ccccccccccccccccccccccccccccccc1";
inline constexpr const char* kSplitA = "n1_ddddddddddddddddddddddddddddddd1";
inline constexpr const char* kSplitB = "n1_ddddddddddddddddddddddddddddddd2";

// Renaming a node re-keys every record whose identity tuple names it: incident
// edges, facts about it, and the evidence records over those subjects are
// removed and re-added under fresh producer-derived IDs (never mutated in
// place).
inline constexpr const char* kEOwnRenamed = "e1_ccccccccccccccccccccccccccccccc1";
inline constexpr const char* kEMutRenamed = "e1_ccccccccccccccccccccccccccccccc2";
inline constexpr const char* kFHandleCapRenamed = "f1_ccccccccccccccccccccccccccccccc1";
inline constexpr const char* kFHandleStateRenamed = "f1_ccccccccccccccccccccccccccccccc2";
inline constexpr const char* kEvHandleRenamed = "v1_ccccccccccccccccccccccccccccccc1";
inline constexpr const char* kEvHandleCapRenamed = "v1_ccccccccccccccccccccccccccccccc2";
inline constexpr const char* kEvHandleStateRenamed = "v1_ccccccccccccccccccccccccccccccc3";
inline constexpr const char* kEvOwnRenamed = "v1_ccccccccccccccccccccccccccccccc4";
inline constexpr const char* kEvMutRenamed = "v1_ccccccccccccccccccccccccccccccc5";

inline constexpr const char* kAnchor = "a1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa0";
inline constexpr const char* kEvProgram = "v1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa0";
inline constexpr const char* kEvProgramFact = "v1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1";
inline constexpr const char* kEvHandle = "v1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2";
inline constexpr const char* kEvHandleCap = "v1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa3";
inline constexpr const char* kEvHandleState = "v1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa4";
inline constexpr const char* kEvStream = "v1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa5";
inline constexpr const char* kEvTask = "v1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa6";
inline constexpr const char* kEvSink = "v1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa7";
inline constexpr const char* kEvTieA = "v1_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1";
inline constexpr const char* kEvTieB = "v1_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2";
inline constexpr const char* kEvFlowA = "v1_eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee1";
inline constexpr const char* kEvFlowB = "v1_eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee2";
inline constexpr const char* kEvFlowSinkA = "v1_eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee3";
inline constexpr const char* kEvFlowSinkB = "v1_eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee4";
inline constexpr const char* kEvOwn = "v1_eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee5";
inline constexpr const char* kEvMut = "v1_eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee6";
inline constexpr const char* kEvFail = "v1_eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee7";
inline constexpr const char* kEvTaskFlow = "v1_eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee8";
inline constexpr const char* kEvRename = "v1_ffffffffffffffffffffffffffffff01";

inline constexpr const char* kEFlowA = "e1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1";
inline constexpr const char* kEFlowB = "e1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2";
inline constexpr const char* kEFlowSinkA = "e1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa3";
inline constexpr const char* kEFlowSinkB = "e1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa4";
inline constexpr const char* kEOwn = "e1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa5";
inline constexpr const char* kEMut = "e1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa6";
inline constexpr const char* kEFail = "e1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa7";
inline constexpr const char* kETask = "e1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa8";

inline constexpr const char* kFProgramCap = "f1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa0";
inline constexpr const char* kFProgramState = "f1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1";
inline constexpr const char* kFHandleCap = "f1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2";
inline constexpr const char* kFHandleState = "f1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa3";
inline constexpr const char* kFStreamCap = "f1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa4";
inline constexpr const char* kFStreamState = "f1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa5";
inline constexpr const char* kFTaskCap = "f1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa6";
inline constexpr const char* kFTaskState = "f1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa7";
inline constexpr const char* kFSinkCap = "f1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa8";
inline constexpr const char* kFSinkState = "f1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa9";
inline constexpr const char* kFTieACap = "f1_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb0";
inline constexpr const char* kFTieAState = "f1_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1";
inline constexpr const char* kFTieBCap = "f1_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2";
inline constexpr const char* kFTieBState = "f1_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb3";
inline constexpr const char* kDiag = "d1_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa0";

inline obs::SnapshotNode make_node(
  std::string id,
  std::string kind,
  std::string role,
  std::string evidence,
  bool anchored = true
) {
  obs::SnapshotNode node;
  node.id = std::move(id);
  node.kind = std::move(kind);
  node.role = std::move(role);
  if (anchored) {
    node.anchors = {kAnchor};
  }
  node.evidence = std::move(evidence);
  return node;
}

inline obs::SnapshotEdge make_edge(
  std::string id,
  std::string kind,
  std::string from,
  std::string to,
  std::string evidence
) {
  obs::SnapshotEdge edge;
  edge.id = std::move(id);
  edge.kind = std::move(kind);
  edge.from = std::move(from);
  edge.to = std::move(to);
  edge.evidence = std::move(evidence);
  return edge;
}

inline obs::SnapshotFact make_fact(
  std::string id,
  std::string subject,
  std::string predicate,
  std::string value,
  std::string evidence,
  bool array = false
) {
  obs::SnapshotFact fact;
  fact.id = std::move(id);
  fact.subject = std::move(subject);
  fact.predicate = std::move(predicate);
  fact.evidence = std::move(evidence);
  if (array) {
    fact.value_is_array = true;
    if (!value.empty()) {
      fact.array_value.push_back(std::move(value));
    }
  } else {
    fact.string_value = std::move(value);
  }
  return fact;
}

inline obs::SnapshotEvidence make_evidence(
  std::string id,
  std::string rule,
  std::vector<std::string> subjects,
  std::vector<std::string> prerequisites = {},
  std::vector<std::string> anchors = {kAnchor}
) {
  obs::SnapshotEvidence evidence;
  evidence.id = std::move(id);
  evidence.producer_rule = std::move(rule);
  evidence.rule_version = "1";
  evidence.subjects = std::move(subjects);
  evidence.prerequisites = std::move(prerequisites);
  evidence.anchors = std::move(anchors);
  return evidence;
}

inline obs::Snapshot base_parent() {
  obs::Snapshot snapshot;
  snapshot.producer = {"styio", "0.0.1"};
  snapshot.compilation_unit = {"example.app", "Styio.toml", "src/main.styio"};
  snapshot.completeness = std::string(obs::kCompletenessValidatedTopology);
  snapshot.root = kProgram;
  snapshot.anchors.push_back({kAnchor, "src/main.styio", "file"});

  snapshot.nodes = {
    make_node(kProgram, "Program", "Program", kEvProgram),
    make_node(kHandle, "Handle", "ResourceHandle", kEvHandle),
    make_node(kStream, "StreamOp", "StreamOperation", kEvStream),
    make_node(kTask, "Task", "Task", kEvTask),
    make_node(kSink, "Sink", "Sink", kEvSink),
    make_node(kTieA, "Value", "Value", kEvTieA),
    make_node(kTieB, "Value", "Value", kEvTieB),
  };
  snapshot.edges = {
    make_edge(kEFlowA, "Flow", kProgram, kTieA, kEvFlowA),
    make_edge(kEFlowB, "Flow", kProgram, kTieB, kEvFlowB),
    make_edge(kEFlowSinkA, "Flow", kTieA, kSink, kEvFlowSinkA),
    make_edge(kEFlowSinkB, "Flow", kTieB, kSink, kEvFlowSinkB),
    make_edge(kEOwn, "Ownership", kProgram, kHandle, kEvOwn),
    make_edge(kEMut, "Mutation", kHandle, kStream, kEvMut),
    make_edge(kEFail, "Failure", kTask, kSink, kEvFail),
    make_edge(kETask, "Flow", kProgram, kTask, kEvTaskFlow),
  };
  snapshot.facts = {
    make_fact(kFProgramCap, kProgram, "capabilities", "", kEvProgramFact, true),
    make_fact(kFProgramState, kProgram, "type-state", "Ready", kEvProgramFact),
    make_fact(kFHandleCap, kHandle, "capabilities", "pull", kEvHandleCap, true),
    make_fact(kFHandleState, kHandle, "type-state", "Unknown", kEvHandleState),
    make_fact(kFStreamCap, kStream, "capabilities", "", kEvStream, true),
    make_fact(kFStreamState, kStream, "type-state", "Ready", kEvStream),
    make_fact(kFTaskCap, kTask, "capabilities", "task", kEvTask, true),
    make_fact(kFTaskState, kTask, "type-state", "Ready", kEvTask),
    make_fact(kFSinkCap, kSink, "capabilities", "", kEvSink, true),
    make_fact(kFSinkState, kSink, "type-state", "Ready", kEvSink),
    make_fact(kFTieACap, kTieA, "capabilities", "", kEvTieA, true),
    make_fact(kFTieAState, kTieA, "type-state", "Unknown", kEvTieA),
    make_fact(kFTieBCap, kTieB, "capabilities", "", kEvTieB, true),
    make_fact(kFTieBState, kTieB, "type-state", "Unknown", kEvTieB),
  };
  snapshot.evidence = {
    make_evidence(kEvProgram, "styio.sema.topology.node.Program", {kProgram}, {}),
    make_evidence(kEvProgramFact, "styio.observable.static.fact.normalize.type-state", {kFProgramState}, {kEvProgram}),
    make_evidence(kEvHandle, "styio.sema.topology.node.ResourceHandle", {kHandle}, {}),
    make_evidence(kEvHandleCap, "styio.observable.static.fact.normalize.capabilities", {kFHandleCap}, {kEvHandle}),
    make_evidence(kEvHandleState, "styio.observable.static.fact.normalize.type-state", {kFHandleState}, {kEvHandle}),
    make_evidence(kEvStream, "styio.sema.topology.node.StreamOperation", {kStream}, {}),
    make_evidence(kEvTask, "styio.sema.topology.node.Task", {kTask}, {}),
    make_evidence(kEvSink, "styio.sema.topology.node.Sink", {kSink}, {}),
    make_evidence(kEvTieA, "styio.sema.topology.node.Value", {kTieA}, {}),
    make_evidence(kEvTieB, "styio.sema.topology.node.Value", {kTieB}, {}),
    make_evidence(kEvFlowA, "styio.sema.topology.relation.Flow.default", {kEFlowA}, {kEvProgram, kEvTieA}, {}),
    make_evidence(kEvFlowB, "styio.sema.topology.relation.Flow.default", {kEFlowB}, {kEvProgram, kEvTieB}, {}),
    make_evidence(kEvFlowSinkA, "styio.sema.topology.relation.Flow.default", {kEFlowSinkA}, {kEvTieA, kEvSink}, {}),
    make_evidence(kEvFlowSinkB, "styio.sema.topology.relation.Flow.default", {kEFlowSinkB}, {kEvTieB, kEvSink}, {}),
    make_evidence(kEvOwn, "styio.sema.topology.relation.Ownership.default", {kEOwn}, {kEvProgram, kEvHandle}, {}),
    make_evidence(kEvMut, "styio.sema.topology.relation.Mutation.default", {kEMut}, {kEvHandle, kEvStream}, {}),
    make_evidence(kEvFail, "styio.sema.topology.relation.Failure.default", {kEFail}, {kEvTask, kEvSink}, {}),
    make_evidence(kEvTaskFlow, "styio.sema.topology.relation.Flow.default", {kETask}, {kEvProgram, kEvTask}, {}),
  };
  const auto finished = obs::finalize_snapshot(snapshot);
  return finished.snapshot;
}

inline obs::Snapshot child_field_change() {
  auto snapshot = base_parent();
  for (auto& fact : snapshot.facts) {
    if (fact.id == kFHandleState) {
      fact.string_value = "Open";
    }
  }
  return obs::finalize_snapshot(snapshot).snapshot;
}

inline obs::Snapshot child_add_diagnostic() {
  auto snapshot = base_parent();
  obs::SnapshotDiagnostic diagnostic;
  diagnostic.id = kDiag;
  diagnostic.code = "STYIO_OBS_EXAMPLE";
  diagnostic.severity = "info";
  diagnostic.subject = kHandle;
  diagnostic.evidence = kEvHandle;
  snapshot.diagnostics.push_back(std::move(diagnostic));
  return obs::finalize_snapshot(snapshot).snapshot;
}

inline obs::Snapshot child_remove_tie_b() {
  auto snapshot = base_parent();
  auto pred_id = [](const auto& record, const char* id) { return record.id == id; };
  snapshot.nodes.erase(
    std::remove_if(snapshot.nodes.begin(), snapshot.nodes.end(),
      [&](const auto& record) { return pred_id(record, kTieB); }),
    snapshot.nodes.end());
  snapshot.edges.erase(
    std::remove_if(snapshot.edges.begin(), snapshot.edges.end(),
      [&](const auto& record) { return record.from == kTieB || record.to == kTieB; }),
    snapshot.edges.end());
  snapshot.facts.erase(
    std::remove_if(snapshot.facts.begin(), snapshot.facts.end(),
      [&](const auto& record) { return record.subject == kTieB; }),
    snapshot.facts.end());
  snapshot.evidence.erase(
    std::remove_if(snapshot.evidence.begin(), snapshot.evidence.end(),
      [&](const auto& record) {
        return std::find(record.subjects.begin(), record.subjects.end(), kTieB) != record.subjects.end()
          || record.id == kEvTieB || record.id == kEvFlowB || record.id == kEvFlowSinkB;
      }),
    snapshot.evidence.end());
  return obs::finalize_snapshot(snapshot).snapshot;
}

inline obs::Snapshot child_unrelated_sink_state() {
  auto snapshot = base_parent();
  for (auto& fact : snapshot.facts) {
    if (fact.id == kFSinkState) {
      fact.string_value = "Closed";
    }
  }
  return obs::finalize_snapshot(snapshot).snapshot;
}

inline void drop_records(std::vector<obs::SnapshotNode>& records, std::vector<std::string> ids) {
  records.erase(
    std::remove_if(records.begin(), records.end(), [&](const auto& record) {
      return std::find(ids.begin(), ids.end(), record.id) != ids.end();
    }),
    records.end());
}
inline void drop_records(std::vector<obs::SnapshotEdge>& records, std::vector<std::string> ids) {
  records.erase(
    std::remove_if(records.begin(), records.end(), [&](const auto& record) {
      return std::find(ids.begin(), ids.end(), record.id) != ids.end();
    }),
    records.end());
}
inline void drop_records(std::vector<obs::SnapshotFact>& records, std::vector<std::string> ids) {
  records.erase(
    std::remove_if(records.begin(), records.end(), [&](const auto& record) {
      return std::find(ids.begin(), ids.end(), record.id) != ids.end();
    }),
    records.end());
}
inline void drop_records(std::vector<obs::SnapshotEvidence>& records, std::vector<std::string> ids) {
  records.erase(
    std::remove_if(records.begin(), records.end(), [&](const auto& record) {
      return std::find(ids.begin(), ids.end(), record.id) != ids.end();
    }),
    records.end());
}

inline obs::Snapshot child_rename_handle() {
  auto snapshot = base_parent();
  drop_records(snapshot.nodes, {kHandle});
  drop_records(snapshot.edges, {kEOwn, kEMut});
  drop_records(snapshot.facts, {kFHandleCap, kFHandleState});
  drop_records(snapshot.evidence, {kEvHandle, kEvHandleCap, kEvHandleState, kEvOwn, kEvMut});
  snapshot.nodes.push_back(make_node(kHandleRenamed, "Handle", "ResourceHandle", kEvHandleRenamed));
  snapshot.edges.push_back(make_edge(kEOwnRenamed, "Ownership", kProgram, kHandleRenamed, kEvOwnRenamed));
  snapshot.edges.push_back(make_edge(kEMutRenamed, "Mutation", kHandleRenamed, kStream, kEvMutRenamed));
  snapshot.facts.push_back(
    make_fact(kFHandleCapRenamed, kHandleRenamed, "capabilities", "pull", kEvHandleCapRenamed, true));
  snapshot.facts.push_back(
    make_fact(kFHandleStateRenamed, kHandleRenamed, "type-state", "Unknown", kEvHandleStateRenamed));
  snapshot.evidence.push_back(make_evidence(
    kEvHandleRenamed, "styio.sema.topology.node.ResourceHandle", {kHandleRenamed}, {}));
  snapshot.evidence.push_back(make_evidence(
    kEvHandleCapRenamed,
    "styio.observable.static.fact.normalize.capabilities",
    {kFHandleCapRenamed},
    {kEvHandleRenamed}));
  snapshot.evidence.push_back(make_evidence(
    kEvHandleStateRenamed,
    "styio.observable.static.fact.normalize.type-state",
    {kFHandleStateRenamed},
    {kEvHandleRenamed}));
  snapshot.evidence.push_back(make_evidence(
    kEvOwnRenamed,
    "styio.sema.topology.relation.Ownership.default",
    {kEOwnRenamed},
    {kEvProgram, kEvHandleRenamed},
    {}));
  snapshot.evidence.push_back(make_evidence(
    kEvMutRenamed,
    "styio.sema.topology.relation.Mutation.default",
    {kEMutRenamed},
    {kEvHandleRenamed, kEvStream},
    {}));
  return obs::finalize_snapshot(snapshot).snapshot;
}

inline obs::Snapshot child_split_stream() {
  auto snapshot = base_parent();
  snapshot.nodes.push_back(make_node(kSplitA, "StreamOp", "StreamOperation", kEvStream));
  snapshot.nodes.push_back(make_node(kSplitB, "StreamOp", "StreamOperation", kEvStream));
  snapshot.facts.push_back(make_fact("f1_ddddddddddddddddddddddddddddddd0", kSplitA, "type-state", "Ready", kEvStream));
  snapshot.facts.push_back(make_fact("f1_ddddddddddddddddddddddddddddddd1", kSplitB, "type-state", "Ready", kEvStream));
  return obs::finalize_snapshot(snapshot).snapshot;
}

inline obs::Snapshot child_merge_ties() {
  auto snapshot = child_remove_tie_b();
  return snapshot;
}

inline obs::Snapshot partial_snapshot() {
  obs::Snapshot snapshot;
  snapshot.producer = {"styio", "0.0.1"};
  snapshot.compilation_unit = {"example.app", "Styio.toml", "src/main.styio"};
  snapshot.completeness = std::string(obs::kCompletenessProvenScalarNoop);
  snapshot.root_is_null = true;
  return obs::finalize_snapshot(snapshot).snapshot;
}

inline obs::Snapshot attach_lineage(obs::Snapshot child, const obs::Snapshot& parent, const obs::LineageDraft& draft) {
  auto constructed = obs::construct_lineage(parent, child, {draft});
  if (constructed.ok) {
    child.lineage = constructed.records;
  }
  return obs::finalize_snapshot(child).snapshot;
}

} // namespace observable_fixtures
