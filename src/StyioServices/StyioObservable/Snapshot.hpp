#pragma once
#ifndef STYIO_OBSERVABLE_SNAPSHOT_HPP_
#define STYIO_OBSERVABLE_SNAPSHOT_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace styio::observable {

inline constexpr int kStaticSnapshotSchemaVersion = 1;
inline constexpr std::string_view kStaticSnapshotContractName =
  "styio.observable.static-snapshot";
inline constexpr std::string_view kStaticSnapshotStability = "incubating";
inline constexpr std::string_view kStaticSnapshotArtifactSuffix =
  ".observable-static-snapshot.json";
inline constexpr std::string_view kCompletenessValidatedTopology =
  "complete/validated-topology";
inline constexpr std::string_view kCompletenessProvenScalarNoop =
  "complete/proven-scalar-noop";
inline constexpr std::string_view kAnchorPrecisionFile = "file";
inline constexpr std::string_view kFactPredicateCapabilities = "capabilities";
inline constexpr std::string_view kFactPredicateTypeState = "type-state";
inline constexpr std::string_view kSnapshotIdPrefix = "s1_";
inline constexpr std::string_view kLineageIdPrefix = "l1_";
inline constexpr std::string_view kDiagnosticIdPrefix = "d1_";

inline constexpr std::string_view kNodeIdPrefix = "n1_";
inline constexpr std::string_view kEdgeIdPrefix = "e1_";
inline constexpr std::string_view kFactIdPrefix = "f1_";
inline constexpr std::string_view kAnchorIdPrefix = "a1_";
inline constexpr std::string_view kEvidenceIdPrefix = "v1_";

inline constexpr std::array<std::string_view, 5> kStaticSnapshotCapabilities{
  "file-source-anchors",
  "producer-evidence",
  "static-topology-edges",
  "static-topology-facts",
  "static-topology-nodes",
};

inline constexpr std::array<std::string_view, 3> kS2OptionalCapabilities{
  "bounded-query",
  "producer-lineage",
  "snapshot-delta",
};

bool static_snapshot_capability_is_supported(std::string_view name) noexcept;
bool observable_capability_is_supported(std::string_view name) noexcept;

struct SnapshotProducer
{
  std::string name = "styio";
  std::string version;
};

struct SnapshotCompilationUnit
{
  std::string package_name;
  std::string manifest_path;
  std::string entry_path;
};

struct SnapshotCounts
{
  std::size_t nodes = 0;
  std::size_t edges = 0;
  std::size_t facts = 0;
  std::size_t diagnostics = 0;
  std::size_t anchors = 0;
  std::size_t evidence = 0;
  std::size_t lineage = 0;
  std::size_t serialized_bytes = 0;
};

struct SnapshotNode
{
  std::string id;
  std::string kind;
  std::string role;
  std::vector<std::string> anchors;
  std::string evidence;
};

struct SnapshotEdge
{
  std::string id;
  std::string kind;
  std::string from;
  std::string to;
  std::string evidence;
};

struct SnapshotFact
{
  std::string id;
  std::string subject;
  std::string predicate;
  bool value_is_array = false;
  std::vector<std::string> array_value;
  std::string string_value;
  std::string evidence;
};

struct SnapshotDiagnostic
{
  std::string id;
  std::string code;
  std::string severity;
  std::string subject;
  std::string evidence;
};

struct SnapshotAnchor
{
  std::string id;
  std::string path;
  std::string precision;
};

struct SnapshotEvidence
{
  std::string id;
  std::string producer_rule;
  std::string rule_version;
  std::vector<std::string> subjects;
  std::vector<std::string> prerequisites;
  std::vector<std::string> anchors;
};

struct LineageRecord
{
  std::string id;
  std::string kind;
  std::vector<std::string> prior;
  std::vector<std::string> target;
  std::string producer_rule;
  std::string rule_version;
  std::vector<std::string> evidence;
  std::string completeness;
};

struct Snapshot
{
  int schema_version = kStaticSnapshotSchemaVersion;
  std::string contract{kStaticSnapshotContractName};
  std::string stability{kStaticSnapshotStability};
  SnapshotProducer producer;
  std::vector<std::string> capabilities;
  SnapshotCompilationUnit compilation_unit;
  std::string completeness;
  std::string root;
  bool root_is_null = false;
  std::vector<SnapshotNode> nodes;
  std::vector<SnapshotEdge> edges;
  std::vector<SnapshotFact> facts;
  std::vector<SnapshotDiagnostic> diagnostics;
  std::vector<SnapshotAnchor> anchors;
  std::vector<SnapshotEvidence> evidence;
  std::vector<LineageRecord> lineage;
  std::string parent_snapshot_id;
};

struct SnapshotIssue
{
  bool ok = false;
  std::string error;
  std::string json;
  std::string snapshot_id;
  Snapshot snapshot;
  SnapshotCounts counts;
};

std::string serialize_snapshot(const Snapshot& snapshot);
std::string snapshot_identity(std::string_view canonical_json);
std::string snapshot_identity(const Snapshot& snapshot);

SnapshotIssue finalize_snapshot(Snapshot snapshot);
SnapshotIssue parse_snapshot(std::string_view json);
bool snapshot_topology_is_conclusive(const Snapshot& snapshot) noexcept;
std::string qualified_scope_key(const Snapshot& snapshot);

const SnapshotNode* find_node(const Snapshot& snapshot, std::string_view id);
const SnapshotEdge* find_edge(const Snapshot& snapshot, std::string_view id);
const SnapshotFact* find_fact(const Snapshot& snapshot, std::string_view id);
const SnapshotDiagnostic* find_diagnostic(const Snapshot& snapshot, std::string_view id);
const SnapshotAnchor* find_anchor(const Snapshot& snapshot, std::string_view id);
const SnapshotEvidence* find_evidence(const Snapshot& snapshot, std::string_view id);
const LineageRecord* find_lineage(const Snapshot& snapshot, std::string_view id);

bool record_exists(const Snapshot& snapshot, std::string_view id);

} // namespace styio::observable

#endif
