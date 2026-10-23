#pragma once
#ifndef STYIO_OBSERVABLE_SERVICE_HPP_
#define STYIO_OBSERVABLE_SERVICE_HPP_

#include "StyioServices/StyioObservable/Query.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace styio::observable {

inline constexpr std::size_t kDefaultMaxSnapshots = 2;
inline constexpr std::size_t kDefaultMaxDeltas = 1;
inline constexpr std::size_t kDefaultMaxSnapshotBytes = 64ull * 1024ull * 1024ull;
inline constexpr std::size_t kDefaultMaxIndexBytes = 64ull * 1024ull * 1024ull;

struct RetentionLimits
{
  std::size_t max_snapshots = kDefaultMaxSnapshots;
  std::size_t max_deltas = kDefaultMaxDeltas;
  std::size_t max_snapshot_bytes = kDefaultMaxSnapshotBytes;
  std::size_t max_index_bytes = kDefaultMaxIndexBytes;

  static RetentionLimits defaults() { return RetentionLimits{}; }
};

struct ServiceCounters
{
  std::size_t input_records = 0;
  std::size_t changed_records = 0;
  std::size_t delta_bytes = 0;
  std::size_t visited_records = 0;
  std::size_t reused_shards = 0;
  std::size_t rebuilt_shards = 0;
  std::size_t retained_snapshot_bytes = 0;
  std::size_t retained_index_bytes = 0;
  std::size_t reference_fallbacks = 0;
};

struct PublicationResult
{
  bool ok = false;
  std::string error;
  std::string reason;
  std::string snapshot_id;
  std::optional<TopologyDelta> delta;
};

class ObservableTopologyService
{
public:
  explicit ObservableTopologyService(
    std::string qualified_scope,
    RetentionLimits limits = RetentionLimits::defaults()
  );

  const std::string& scope() const noexcept { return scope_; }
  const RetentionLimits& limits() const noexcept { return limits_; }

  NegotiationResult negotiate(const NegotiationRequest& request) const;
  PublicationResult publish(const Snapshot& snapshot);
  QueryResponse query(const QueryRequest& request);
  QueryResponse query_parent(const QueryRequest& request);

  std::optional<Snapshot> current() const;
  std::optional<Snapshot> parent() const;
  std::optional<TopologyDelta> delta() const;
  bool index_present() const noexcept;
  void drop_derived_indexes();
  ServiceCounters counters() const;
  const IndexShard* shard(std::string_view subject) const;

private:
  struct State
  {
    std::optional<Snapshot> current;
    std::optional<Snapshot> parent;
    std::optional<TopologyDelta> delta;
    std::optional<SnapshotIndex> current_index;
    std::optional<SnapshotIndex> parent_index;
    std::size_t snapshot_bytes = 0;
    std::size_t index_bytes = 0;
  };

  std::string scope_;
  RetentionLimits limits_;
  State state_;
  ServiceCounters counters_;
};

} // namespace styio::observable

#endif
