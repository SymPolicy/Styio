#include "Service.hpp"

namespace styio::observable {
namespace {

std::size_t snapshot_record_count(const Snapshot& snapshot) {
  return snapshot.nodes.size() + snapshot.edges.size() + snapshot.facts.size()
    + snapshot.diagnostics.size() + snapshot.anchors.size() + snapshot.evidence.size()
    + snapshot.lineage.size();
}

} // namespace

ObservableTopologyService::ObservableTopologyService(
  std::string qualified_scope,
  RetentionLimits limits
)
  : scope_(std::move(qualified_scope)), limits_(limits)
{
  if (limits_.max_snapshots == 0) {
    limits_.max_snapshots = kDefaultMaxSnapshots;
  }
  if (limits_.max_deltas == 0) {
    limits_.max_deltas = kDefaultMaxDeltas;
  }
  if (limits_.max_snapshot_bytes == 0) {
    limits_.max_snapshot_bytes = kDefaultMaxSnapshotBytes;
  }
  if (limits_.max_index_bytes == 0) {
    limits_.max_index_bytes = kDefaultMaxIndexBytes;
  }
}

NegotiationResult ObservableTopologyService::negotiate(const NegotiationRequest& request) const {
  return negotiate_observable_contracts(request);
}

PublicationResult ObservableTopologyService::publish(const Snapshot& snapshot) {
  PublicationResult result;
  auto finished = finalize_snapshot(snapshot);
  if (!finished.ok) {
    result.error = finished.error;
    result.reason = std::string(kReasonInvalid);
    return result;
  }
  if (qualified_scope_key(finished.snapshot) != scope_) {
    result.error = "snapshot compilation unit does not match the service scope";
    result.reason = std::string(kReasonInvalid);
    return result;
  }

  State candidate = state_;
  std::optional<TopologyDelta> generated;
  if (candidate.current.has_value()) {
    auto delta = generate_delta(*candidate.current, finished.snapshot);
    if (!delta.ok) {
      result.error = delta.error;
      result.reason = delta.reason.empty() ? std::string(kReasonInvalid) : delta.reason;
      return result;
    }
    auto applied = apply_delta(*candidate.current, delta.delta);
    if (!applied.ok) {
      result.error = applied.error;
      result.reason = applied.reason.empty() ? std::string(kReasonInvalid) : applied.reason;
      return result;
    }
    generated = std::move(delta.delta);
    candidate.parent = candidate.current;
    candidate.parent_index = candidate.current_index;
    candidate.delta = generated;
    if (candidate.current_index.has_value() && generated.has_value()) {
      candidate.current_index = merge_snapshot_index(
        *candidate.current_index,
        finished.snapshot,
        *generated);
    } else {
      candidate.current_index = build_snapshot_index(finished.snapshot);
    }
  } else {
    candidate.current_index = build_snapshot_index(finished.snapshot);
    candidate.delta.reset();
    candidate.parent.reset();
    candidate.parent_index.reset();
  }
  candidate.current = finished.snapshot;
  if (limits_.max_snapshots <= 1) {
    // Eviction drops the parent snapshot, its index shards, and the dependent
    // parent-to-current delta together.
    candidate.parent.reset();
    candidate.parent_index.reset();
    candidate.delta.reset();
  }

  std::size_t snapshot_bytes = 0;
  if (candidate.current) {
    snapshot_bytes += serialize_snapshot(*candidate.current).size();
  }
  if (candidate.parent) {
    snapshot_bytes += serialize_snapshot(*candidate.parent).size();
  }
  std::size_t index_bytes = 0;
  if (candidate.current_index) {
    index_bytes += candidate.current_index->byte_size;
  }
  if (candidate.parent_index) {
    index_bytes += candidate.parent_index->byte_size;
  }
  if (snapshot_bytes > limits_.max_snapshot_bytes || index_bytes > limits_.max_index_bytes) {
    result.error = "publication exceeds retention byte limits";
    result.reason = std::string(kReasonResourceLimit);
    return result;
  }

  candidate.snapshot_bytes = snapshot_bytes;
  candidate.index_bytes = index_bytes;
  state_ = std::move(candidate);

  counters_.input_records = snapshot_record_count(finished.snapshot);
  counters_.changed_records = generated.has_value() ? generated->operations.size() : 0;
  counters_.delta_bytes = generated.has_value() ? serialize_delta(*generated).size() : 0;
  counters_.reused_shards = state_.current_index ? state_.current_index->reused_shards : 0;
  counters_.rebuilt_shards = state_.current_index ? state_.current_index->rebuilt_shards : 0;
  counters_.retained_snapshot_bytes = state_.snapshot_bytes;
  counters_.retained_index_bytes = state_.index_bytes;

  result.ok = true;
  result.snapshot_id = finished.snapshot_id;
  result.delta = std::move(generated);
  return result;
}

QueryResponse ObservableTopologyService::query(const QueryRequest& request) {
  QueryResponse response;
  if (!state_.current.has_value()) {
    response.status = QueryStatus::Invalid;
    response.reason = std::string(kReasonFullSnapshotRequired);
    return response;
  }
  if (state_.current_index.has_value()) {
    response = evaluate_query_index(*state_.current, *state_.current_index, request);
  } else {
    response = evaluate_query_reference(*state_.current, request);
    ++counters_.reference_fallbacks;
  }
  counters_.visited_records = response.visited;
  return response;
}

QueryResponse ObservableTopologyService::query_parent(const QueryRequest& request) {
  QueryResponse response;
  if (!state_.parent.has_value()) {
    response.status = QueryStatus::Invalid;
    response.reason = std::string(kReasonFullSnapshotRequired);
    if (state_.current.has_value()) {
      response.snapshot_id = snapshot_identity(*state_.current);
      response.completeness = state_.current->completeness;
    }
    return response;
  }
  if (state_.parent_index.has_value()) {
    response = evaluate_query_index(*state_.parent, *state_.parent_index, request);
  } else {
    response = evaluate_query_reference(*state_.parent, request);
    ++counters_.reference_fallbacks;
  }
  counters_.visited_records = response.visited;
  return response;
}

std::optional<Snapshot> ObservableTopologyService::current() const {
  return state_.current;
}

std::optional<Snapshot> ObservableTopologyService::parent() const {
  return state_.parent;
}

std::optional<TopologyDelta> ObservableTopologyService::delta() const {
  return state_.delta;
}

bool ObservableTopologyService::index_present() const noexcept {
  return state_.current_index.has_value();
}

void ObservableTopologyService::drop_derived_indexes() {
  state_.current_index.reset();
  state_.parent_index.reset();
  state_.index_bytes = 0;
  counters_.retained_index_bytes = 0;
}

ServiceCounters ObservableTopologyService::counters() const {
  return counters_;
}

const IndexShard* ObservableTopologyService::shard(std::string_view subject) const {
  if (!state_.current_index.has_value()) {
    return nullptr;
  }
  return find_shard(*state_.current_index, subject);
}

} // namespace styio::observable
