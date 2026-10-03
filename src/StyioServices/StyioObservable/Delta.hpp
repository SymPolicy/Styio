#pragma once
#ifndef STYIO_OBSERVABLE_DELTA_HPP_
#define STYIO_OBSERVABLE_DELTA_HPP_

#include "StyioServices/StyioObservable/Snapshot.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace styio::observable {

inline constexpr std::string_view kDeltaContractName = "styio.observable.delta";
inline constexpr int kDeltaSchemaMajor = 0;
inline constexpr int kDeltaSchemaMinor = 1;
inline constexpr std::string_view kLineageCapability = "producer-lineage";
inline constexpr std::string_view kDeltaCapability = "snapshot-delta";

inline constexpr std::string_view kReasonInvalid = "invalid";
inline constexpr std::string_view kReasonUnsupported = "unsupported";
inline constexpr std::string_view kReasonFullSnapshotRequired = "full_snapshot_required";
inline constexpr std::string_view kReasonUnknownRequiredCapability = "unknown_required_capability";
inline constexpr std::string_view kReasonMajorIncompatible = "major_incompatible";
inline constexpr std::string_view kReasonResourceLimit = "resource_limit";
inline constexpr std::string_view kReasonWrongBase = "wrong_base";
inline constexpr std::string_view kReasonMalformedOperation = "malformed_operation";

enum class RecordCategory : std::uint8_t
{
  Metadata,
  Nodes,
  Edges,
  Facts,
  Diagnostics,
  Anchors,
  Evidence,
  Lineage,
};

enum class DeltaOpKind : std::uint8_t
{
  Add,
  Remove,
  ReplaceFields,
};

struct FieldReplacement
{
  std::string name;
  std::string before;
  std::string after;
};

struct DeltaOperation
{
  DeltaOpKind kind = DeltaOpKind::Add;
  RecordCategory category = RecordCategory::Nodes;
  std::string key;
  std::string record_json;
  std::vector<FieldReplacement> fields;
};

struct TopologyDelta
{
  std::string contract{kDeltaContractName};
  int schema_major = kDeltaSchemaMajor;
  int schema_minor = kDeltaSchemaMinor;
  std::string stability{"incubating"};
  std::string parent_snapshot_id;
  std::string target_snapshot_id;
  std::vector<std::string> required_capabilities;
  std::vector<std::string> optional_capabilities;
  std::vector<DeltaOperation> operations;
};

struct DeltaIssue
{
  bool ok = false;
  std::string error;
  std::string reason{kReasonInvalid};
  TopologyDelta delta;
  std::string json;
  Snapshot snapshot;
  std::string snapshot_id;
};

struct LineageDraft
{
  std::string kind;
  std::vector<std::string> prior;
  std::vector<std::string> target;
  std::string producer_rule;
  std::string rule_version;
  std::vector<std::string> evidence;
  std::string completeness{"complete"};
};

struct LineageIssue
{
  bool ok = false;
  std::string error;
  std::vector<LineageRecord> records;
};

std::string_view category_name(RecordCategory category) noexcept;
std::optional<RecordCategory> parse_category(std::string_view name);
std::string_view op_name(DeltaOpKind kind) noexcept;

std::string serialize_delta(const TopologyDelta& delta);
DeltaIssue parse_delta(std::string_view json);
DeltaIssue generate_delta(const Snapshot& parent, const Snapshot& child);
DeltaIssue apply_delta(const Snapshot& parent, const TopologyDelta& delta);
std::vector<std::string> delta_seed_keys(const TopologyDelta& delta);

LineageIssue construct_lineage(
  const Snapshot& parent,
  const Snapshot& child,
  const std::vector<LineageDraft>& drafts);

bool producer_rule_is_canonical(std::string_view rule) noexcept;

} // namespace styio::observable

#endif
