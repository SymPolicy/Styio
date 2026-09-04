#pragma once
#ifndef STYIO_OBSERVABLE_QUERY_HPP_
#define STYIO_OBSERVABLE_QUERY_HPP_

#include "StyioServices/StyioObservable/Delta.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace styio::observable {

inline constexpr std::string_view kQueryContractName = "styio.observable.query";
inline constexpr int kQuerySchemaMajor = 0;
inline constexpr int kQuerySchemaMinor = 1;
inline constexpr std::string_view kQueryCapability = "bounded-query";

inline constexpr std::size_t kQueryDefaultResults = 256;
inline constexpr std::size_t kQueryDefaultDepth = 16;
inline constexpr std::size_t kQueryDefaultVisited = 4096;
inline constexpr std::size_t kQueryDefaultEvidence = 1024;
inline constexpr std::size_t kQueryHardResults = 4096;
inline constexpr std::size_t kQueryHardDepth = 64;
inline constexpr std::size_t kQueryHardVisited = 65536;
inline constexpr std::size_t kQueryHardEvidence = 8192;

enum class QueryKind : std::uint8_t
{
  Lookup,
  Dependencies,
  Dependents,
  Effects,
  Ownership,
  Mutation,
  Failure,
  TaskScope,
  StreamScope,
  Impact,
  CanonicalPath,
  Lineage,
  Why,
};

enum class QueryStatus : std::uint8_t
{
  Complete,
  Partial,
  Truncated,
  Unsupported,
  Invalid,
};

struct QueryLimits
{
  std::size_t max_results = kQueryDefaultResults;
  std::size_t max_depth = kQueryDefaultDepth;
  std::size_t max_visited = kQueryDefaultVisited;
  std::size_t max_evidence = kQueryDefaultEvidence;
};

struct ContractVersion
{
  int major = 0;
  int minor = 1;
};

struct NegotiationRequest
{
  int snapshot_schema = kStaticSnapshotSchemaVersion;
  ContractVersion delta{kDeltaSchemaMajor, kDeltaSchemaMinor};
  ContractVersion query{kQuerySchemaMajor, kQuerySchemaMinor};
  ContractVersion lineage{0, 1};
  std::vector<std::string> required_capabilities;
  std::vector<std::string> optional_capabilities;
};

struct NegotiationResult
{
  bool ok = false;
  std::string reason;
  int snapshot_schema = kStaticSnapshotSchemaVersion;
  ContractVersion delta;
  ContractVersion query;
  ContractVersion lineage;
  std::vector<std::string> capabilities;
};

struct QueryRequest
{
  std::string contract{kQueryContractName};
  ContractVersion schema{kQuerySchemaMajor, kQuerySchemaMinor};
  QueryKind kind = QueryKind::Lookup;
  std::string subject;
  std::string target;
  std::vector<std::string> relation_kinds;
  QueryLimits limits;
  std::vector<std::string> required_capabilities;
  std::vector<std::string> optional_capabilities;
};

struct QueryResultRecord
{
  std::string category;
  std::string id;
  std::string json;
};

struct QueryResponse
{
  std::string snapshot_id;
  QueryStatus status = QueryStatus::Invalid;
  std::string completeness;
  std::string reason;
  std::vector<std::string> negotiated_capabilities;
  std::vector<QueryResultRecord> results;
  std::vector<std::string> evidence;
  std::size_t visited = 0;
  std::size_t evidence_count = 0;
  bool truncated = false;
};

// Immutable per-subject index shard. All lists hold canonical record IDs of
// the owning snapshot, sorted ascending: incident edge IDs per direction,
// fact IDs whose subject is this node, and lineage record IDs mentioning this
// node. Record payloads are never duplicated here; evaluation resolves IDs
// against the retained snapshot. `dependency_keys` is the sorted unique set of
// record IDs whose change or removal invalidates this shard.
struct IndexShard
{
  std::string subject_id;
  std::vector<std::string> dependency_keys;
  std::vector<std::string> outgoing_edges;
  std::vector<std::string> incoming_edges;
  std::vector<std::string> facts;
  std::vector<std::string> lineage;
  std::size_t byte_size = 0;
};

struct SnapshotIndex
{
  std::string snapshot_id;
  std::vector<std::shared_ptr<const IndexShard>> shards;
  std::unordered_map<std::string, std::size_t> shard_by_subject;
  std::unordered_map<std::string, std::vector<std::string>> reverse_deps;
  std::size_t byte_size = 0;
  std::size_t reused_shards = 0;
  std::size_t rebuilt_shards = 0;
};

QueryLimits clamp_query_limits(QueryLimits limits);
std::string_view query_kind_name(QueryKind kind) noexcept;
std::optional<QueryKind> parse_query_kind(std::string_view name);
std::string_view query_status_name(QueryStatus status) noexcept;

NegotiationResult negotiate_observable_contracts(const NegotiationRequest& request);
QueryRequest parse_query_request(std::string_view json, std::string& error);
std::string serialize_query_response(const QueryResponse& response);

QueryResponse evaluate_query_reference(const Snapshot& snapshot, const QueryRequest& request);
SnapshotIndex build_snapshot_index(const Snapshot& snapshot);
// Merge-reuses parent shards whose subject is outside the invalidation
// closure of the delta seed set (changed record keys plus the endpoints of
// added edges, facts, and lineage); every other shard is rebuilt from the
// child. The delta must have passed `apply_delta` validation first.
SnapshotIndex merge_snapshot_index(
  const SnapshotIndex& parent_index,
  const Snapshot& child,
  const TopologyDelta& delta);
// Optional instrumentation proving that an indexed evaluation actually
// consulted shards instead of re-scanning the snapshot.
struct QueryIndexProbe
{
  std::size_t shard_lookups = 0;
  std::size_t shard_candidates = 0;
};
QueryResponse evaluate_query_index(
  const Snapshot& snapshot,
  const SnapshotIndex& index,
  const QueryRequest& request,
  QueryIndexProbe* probe = nullptr);
const IndexShard* find_shard(const SnapshotIndex& index, std::string_view subject);

} // namespace styio::observable

#endif
