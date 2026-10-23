#include "Query.hpp"

#include "JsonSupport.hpp"

#include <algorithm>
#include <array>
#include <queue>
#include <unordered_set>
#include <utility>

namespace styio::observable {
namespace {

using json_detail::CompactJson;

constexpr std::array<std::string_view, 5> kEffectKinds{
  "Flow", "Intent", "Mutation", "Commit", "Backpressure"};
constexpr std::array<std::string_view, 2> kOwnershipKinds{"Ownership", "Borrow"};
constexpr std::array<std::string_view, 4> kImpactDefaultKinds{
  "Flow", "Ownership", "Mutation", "Intent"};

bool kind_in(std::string_view kind, const std::vector<std::string>& allowed) {
  return std::find(allowed.begin(), allowed.end(), kind) != allowed.end();
}

template <std::size_t N>
std::vector<std::string> to_vec(const std::array<std::string_view, N>& values) {
  std::vector<std::string> out;
  out.reserve(N);
  for (const auto value : values) {
    out.emplace_back(value);
  }
  return out;
}

struct Adjacency
{
  std::unordered_map<std::string, std::vector<const SnapshotEdge*>> outgoing;
  std::unordered_map<std::string, std::vector<const SnapshotEdge*>> incoming;
};

Adjacency build_adjacency(const Snapshot& snapshot) {
  Adjacency adj;
  for (const auto& edge : snapshot.edges) {
    adj.outgoing[edge.from].push_back(&edge);
    adj.incoming[edge.to].push_back(&edge);
  }
  auto sort_edges = [](std::vector<const SnapshotEdge*>& edges) {
    std::sort(edges.begin(), edges.end(), [](const SnapshotEdge* lhs, const SnapshotEdge* rhs) {
      return lhs->id < rhs->id;
    });
  };
  for (auto& [_, edges] : adj.outgoing) {
    sort_edges(edges);
  }
  for (auto& [_, edges] : adj.incoming) {
    sort_edges(edges);
  }
  return adj;
}

// Canonical snapshots store every category sorted by id, so record resolution
// is a binary search. Callers obtain snapshots only from finalize_snapshot or
// parse_snapshot, which both canonicalize ordering.
template <typename T>
const T* find_sorted(const std::vector<T>& records, std::string_view id) {
  const auto it = std::lower_bound(
    records.begin(),
    records.end(),
    id,
    [](const T& record, std::string_view wanted) { return record.id < wanted; });
  if (it == records.end() || it->id != id) {
    return nullptr;
  }
  return &*it;
}

QueryResultRecord make_result(std::string category, std::string id, std::string json) {
  QueryResultRecord record;
  record.category = std::move(category);
  record.id = std::move(id);
  record.json = std::move(json);
  return record;
}

std::string node_json(const SnapshotNode& node) {
  CompactJson json;
  json.begin_object();
  json.key("id");
  json.string_value(node.id);
  json.key("kind");
  json.string_value(node.kind);
  json.key("role");
  json.string_value(node.role);
  json.end_object();
  return json.buf;
}

std::string edge_json(const SnapshotEdge& edge) {
  CompactJson json;
  json.begin_object();
  json.key("id");
  json.string_value(edge.id);
  json.key("kind");
  json.string_value(edge.kind);
  json.key("from");
  json.string_value(edge.from);
  json.key("to");
  json.string_value(edge.to);
  json.end_object();
  return json.buf;
}

std::string fact_json(const SnapshotFact& fact) {
  CompactJson json;
  json.begin_object();
  json.key("id");
  json.string_value(fact.id);
  json.key("subject");
  json.string_value(fact.subject);
  json.key("predicate");
  json.string_value(fact.predicate);
  json.end_object();
  return json.buf;
}

std::string lineage_json(const LineageRecord& lineage) {
  CompactJson json;
  json.begin_object();
  json.key("id");
  json.string_value(lineage.id);
  json.key("kind");
  json.string_value(lineage.kind);
  json.end_object();
  return json.buf;
}

void sort_results(std::vector<QueryResultRecord>& results) {
  std::sort(results.begin(), results.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.category != rhs.category) {
      return lhs.category < rhs.category;
    }
    return lhs.id < rhs.id;
  });
}

void append_unique(std::vector<std::string>& values, const std::string& value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

bool consume_visit(std::size_t& visited, std::size_t max_visited, std::size_t depth, std::size_t max_depth) {
  if (visited >= max_visited || depth > max_depth) {
    return false;
  }
  ++visited;
  return true;
}

QueryResponse finish_response(
  const Snapshot& snapshot,
  std::string snapshot_id,
  const QueryRequest& request,
  std::vector<QueryResultRecord> results,
  std::vector<std::string> evidence,
  std::size_t visited,
  bool truncated,
  bool missing_subject,
  bool preserve_order = false
) {
  QueryResponse response;
  response.snapshot_id = std::move(snapshot_id);
  response.completeness = snapshot.completeness;
  response.visited = visited;
  std::sort(evidence.begin(), evidence.end());
  evidence.erase(std::unique(evidence.begin(), evidence.end()), evidence.end());
  if (evidence.size() > request.limits.max_evidence) {
    evidence.resize(request.limits.max_evidence);
    truncated = true;
  }
  response.evidence = std::move(evidence);
  response.evidence_count = response.evidence.size();
  if (!preserve_order) {
    sort_results(results);
  }
  if (results.size() > request.limits.max_results) {
    results.resize(request.limits.max_results);
    truncated = true;
  }
  response.results = std::move(results);
  response.truncated = truncated;
  if (truncated) {
    response.status = QueryStatus::Truncated;
    response.reason = "truncated";
  } else if (missing_subject && !snapshot_topology_is_conclusive(snapshot)) {
    response.status = QueryStatus::Partial;
    response.reason = "partial";
  } else {
    response.status = QueryStatus::Complete;
  }
  return response;
}

std::vector<std::string> relation_filter(const QueryRequest& request, QueryKind kind) {
  if (!request.relation_kinds.empty()) {
    return request.relation_kinds;
  }
  switch (kind) {
    case QueryKind::Effects: return to_vec(kEffectKinds);
    case QueryKind::Ownership: return to_vec(kOwnershipKinds);
    case QueryKind::Mutation: return {"Mutation"};
    case QueryKind::Failure: return {"Failure"};
    case QueryKind::Impact: return to_vec(kImpactDefaultKinds);
    default: return {};
  }
}

// A query provider locates the candidate records of one query kind. The
// reference provider scans the full snapshot; the index provider consults
// immutable per-subject shards and resolves record payloads from the retained
// canonical snapshot. Both must yield identical candidate sequences so the
// shared evaluator below produces identical answers and visited counts.
struct ReferenceProvider
{
  const Snapshot& snapshot;
  Adjacency adj;
  mutable std::unordered_map<std::string, const SnapshotEvidence*> evidence_by_id;

  explicit ReferenceProvider(const Snapshot& snapshot)
    : snapshot(snapshot), adj(build_adjacency(snapshot))
  {
  }

  const SnapshotNode* node(std::string_view id) const { return find_node(snapshot, id); }
  const SnapshotEdge* edge(std::string_view id) const { return find_edge(snapshot, id); }
  const SnapshotFact* fact(std::string_view id) const { return find_fact(snapshot, id); }
  const LineageRecord* lineage(std::string_view id) const { return find_lineage(snapshot, id); }

  const SnapshotEvidence* evidence(std::string_view id) const {
    if (evidence_by_id.empty() && !snapshot.evidence.empty()) {
      for (const auto& record : snapshot.evidence) {
        evidence_by_id.emplace(record.id, &record);
      }
    }
    const auto it = evidence_by_id.find(std::string(id));
    return it == evidence_by_id.end() ? nullptr : it->second;
  }

  std::vector<const SnapshotEdge*> outgoing(std::string_view id) const {
    const auto it = adj.outgoing.find(std::string(id));
    return it == adj.outgoing.end() ? std::vector<const SnapshotEdge*>{} : it->second;
  }

  std::vector<const SnapshotEdge*> incoming(std::string_view id) const {
    const auto it = adj.incoming.find(std::string(id));
    return it == adj.incoming.end() ? std::vector<const SnapshotEdge*>{} : it->second;
  }

  std::vector<const LineageRecord*> lineage_candidates(std::string_view subject) const {
    std::vector<const LineageRecord*> matches;
    for (const auto& record : snapshot.lineage) {
      const bool match = std::find(record.prior.begin(), record.prior.end(), subject)
          != record.prior.end()
        || std::find(record.target.begin(), record.target.end(), subject)
          != record.target.end();
      if (match) {
        matches.push_back(&record);
      }
    }
    return matches;
  }
};

struct IndexProvider
{
  const Snapshot& snapshot;
  const SnapshotIndex& index;
  QueryIndexProbe* probe;

  const IndexShard* shard_for(std::string_view id) const {
    if (probe != nullptr) {
      probe->shard_lookups += 1;
    }
    return find_shard(index, id);
  }

  const SnapshotNode* node(std::string_view id) const {
    if (shard_for(id) == nullptr) {
      return nullptr;
    }
    return find_sorted(snapshot.nodes, id);
  }

  const SnapshotEdge* edge(std::string_view id) const { return find_sorted(snapshot.edges, id); }
  const SnapshotFact* fact(std::string_view id) const { return find_sorted(snapshot.facts, id); }
  const LineageRecord* lineage(std::string_view id) const { return find_sorted(snapshot.lineage, id); }
  const SnapshotEvidence* evidence(std::string_view id) const { return find_sorted(snapshot.evidence, id); }

  std::vector<const SnapshotEdge*> resolve_edges(const std::vector<std::string>& edge_ids) const {
    std::vector<const SnapshotEdge*> edges;
    edges.reserve(edge_ids.size());
    for (const auto& id : edge_ids) {
      if (const auto* edge = find_sorted(snapshot.edges, id); edge != nullptr) {
        edges.push_back(edge);
      }
    }
    return edges;
  }

  std::vector<const SnapshotEdge*> outgoing(std::string_view id) const {
    const auto* shard = shard_for(id);
    if (shard == nullptr) {
      return {};
    }
    if (probe != nullptr) {
      probe->shard_candidates += shard->outgoing_edges.size();
    }
    return resolve_edges(shard->outgoing_edges);
  }

  std::vector<const SnapshotEdge*> incoming(std::string_view id) const {
    const auto* shard = shard_for(id);
    if (shard == nullptr) {
      return {};
    }
    if (probe != nullptr) {
      probe->shard_candidates += shard->incoming_edges.size();
    }
    return resolve_edges(shard->incoming_edges);
  }

  std::vector<const LineageRecord*> lineage_candidates(std::string_view subject) const {
    if (const auto* shard = shard_for(subject); shard != nullptr) {
      if (probe != nullptr) {
        probe->shard_candidates += shard->lineage.size();
      }
      std::vector<const LineageRecord*> matches;
      matches.reserve(shard->lineage.size());
      for (const auto& id : shard->lineage) {
        if (const auto* record = find_sorted(snapshot.lineage, id); record != nullptr) {
          matches.push_back(record);
        }
      }
      return matches;
    }
    // Lineage subjects are nodes in every producer-emitted record; a non-node
    // subject has no shard, so locate its candidates by the same filtered
    // full scan the reference provider performs.
    std::vector<const LineageRecord*> matches;
    for (const auto& record : snapshot.lineage) {
      const bool match = std::find(record.prior.begin(), record.prior.end(), subject)
          != record.prior.end()
        || std::find(record.target.begin(), record.target.end(), subject)
          != record.target.end();
      if (match) {
        matches.push_back(&record);
      }
    }
    return matches;
  }
};

template <typename P>
void collect_evidence(P& provider, const std::string& id, std::vector<std::string>& evidence) {
  if (const auto* node = provider.node(id); node != nullptr) {
    append_unique(evidence, node->evidence);
  }
  if (const auto* edge = provider.edge(id); edge != nullptr) {
    append_unique(evidence, edge->evidence);
  }
  if (const auto* fact = provider.fact(id); fact != nullptr) {
    append_unique(evidence, fact->evidence);
  }
}

template <typename P>
QueryResponse lookup_query(
  const Snapshot& snapshot,
  std::string snapshot_id,
  P& provider,
  const QueryRequest& request
) {
  std::vector<QueryResultRecord> results;
  std::vector<std::string> evidence;
  bool missing = false;
  if (const auto* node = provider.node(request.subject); node != nullptr) {
    results.push_back(make_result("nodes", node->id, node_json(*node)));
    collect_evidence(provider, node->id, evidence);
  } else if (const auto* edge = provider.edge(request.subject); edge != nullptr) {
    results.push_back(make_result("edges", edge->id, edge_json(*edge)));
    collect_evidence(provider, edge->id, evidence);
  } else if (const auto* fact = provider.fact(request.subject); fact != nullptr) {
    results.push_back(make_result("facts", fact->id, fact_json(*fact)));
    collect_evidence(provider, fact->id, evidence);
  } else if (const auto* lineage = provider.lineage(request.subject); lineage != nullptr) {
    results.push_back(make_result("lineage", lineage->id, lineage_json(*lineage)));
    evidence.insert(evidence.end(), lineage->evidence.begin(), lineage->evidence.end());
  } else {
    missing = true;
  }
  return finish_response(
    snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), 1, false, missing);
}

template <typename P>
QueryResponse edge_query(
  const Snapshot& snapshot,
  std::string snapshot_id,
  P& provider,
  const QueryRequest& request,
  bool outgoing
) {
  const auto* node = provider.node(request.subject);
  std::vector<QueryResultRecord> results;
  std::vector<std::string> evidence;
  std::size_t visited = 1;
  const bool missing = node == nullptr;
  if (node != nullptr) {
    const auto filter = relation_filter(request, request.kind);
    const auto edges = outgoing ? provider.outgoing(node->id) : provider.incoming(node->id);
    for (const auto* edge : edges) {
      if (!consume_visit(visited, request.limits.max_visited, 0, request.limits.max_depth)) {
        return finish_response(
          snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, true, false);
      }
      if (!filter.empty() && !kind_in(edge->kind, filter)) {
        continue;
      }
      results.push_back(make_result("edges", edge->id, edge_json(*edge)));
      collect_evidence(provider, edge->id, evidence);
      if (results.size() >= request.limits.max_results) {
        return finish_response(
          snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, true, false);
      }
    }
  }
  return finish_response(
    snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, false, missing);
}

template <typename P>
QueryResponse filtered_edge_query(
  const Snapshot& snapshot,
  std::string snapshot_id,
  P& provider,
  const QueryRequest& request
) {
  const auto* node = provider.node(request.subject);
  std::vector<QueryResultRecord> results;
  std::vector<std::string> evidence;
  std::size_t visited = 1;
  const bool missing = node == nullptr;
  if (node != nullptr) {
    const auto filter = relation_filter(request, request.kind);
    auto consider = [&](const std::vector<const SnapshotEdge*>& edges) {
      for (const auto* edge : edges) {
        if (!consume_visit(visited, request.limits.max_visited, 0, request.limits.max_depth)) {
          return false;
        }
        if (!kind_in(edge->kind, filter)) {
          continue;
        }
        results.push_back(make_result("edges", edge->id, edge_json(*edge)));
        collect_evidence(provider, edge->id, evidence);
      }
      return true;
    };
    bool within_budget = consider(provider.outgoing(node->id));
    if (within_budget) {
      within_budget = consider(provider.incoming(node->id));
    }
    if (!within_budget || results.size() > request.limits.max_results) {
      return finish_response(
        snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, true, false);
    }
  }
  return finish_response(
    snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, false, missing);
}

template <typename P>
QueryResponse scope_query(
  const Snapshot& snapshot,
  std::string snapshot_id,
  P& provider,
  const QueryRequest& request,
  bool stream
) {
  const auto* start = provider.node(request.subject);
  std::vector<QueryResultRecord> results;
  std::vector<std::string> evidence;
  if (start == nullptr) {
    return finish_response(
      snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), 1, false, true);
  }
  std::unordered_set<std::string> seen;
  std::queue<std::pair<std::string, std::size_t>> queue;
  queue.push({start->id, 0});
  seen.insert(start->id);
  std::size_t visited = 0;
  while (!queue.empty()) {
    const auto [id, depth] = queue.front();
    queue.pop();
    if (!consume_visit(visited, request.limits.max_visited, depth, request.limits.max_depth)) {
      return finish_response(
        snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, true, false);
    }
    const auto* node = provider.node(id);
    if (node == nullptr) {
      continue;
    }
    const bool in_scope = stream
      ? (node->kind == "StreamOp" || node->role == "StreamOperation" || id == start->id)
      : (node->kind == "Task" || node->kind == "FailureDomain" || node->role == "Task"
         || node->role == "TaskFailureDomain" || id == start->id);
    if (in_scope) {
      results.push_back(make_result("nodes", node->id, node_json(*node)));
      collect_evidence(provider, node->id, evidence);
    }
    auto expand = [&](const std::vector<const SnapshotEdge*>& edges) {
      for (const auto* edge : edges) {
        const std::string next = edge->from == id ? edge->to : edge->from;
        if (seen.insert(next).second) {
          queue.push({next, depth + 1});
        }
      }
    };
    expand(provider.outgoing(id));
    expand(provider.incoming(id));
    if (results.size() >= request.limits.max_results) {
      return finish_response(
        snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, true, false);
    }
  }
  return finish_response(
    snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, false, false);
}

template <typename P>
QueryResponse impact_query(
  const Snapshot& snapshot,
  std::string snapshot_id,
  P& provider,
  const QueryRequest& request
) {
  const auto* start = provider.node(request.subject);
  std::vector<QueryResultRecord> results;
  std::vector<std::string> evidence;
  if (start == nullptr) {
    return finish_response(
      snapshot, std::move(snapshot_id), request, {}, {}, 1, false, true);
  }
  const auto filter = relation_filter(request, QueryKind::Impact);
  std::unordered_set<std::string> seen;
  std::queue<std::pair<std::string, std::size_t>> queue;
  queue.push({start->id, 0});
  seen.insert(start->id);
  std::size_t visited = 0;
  while (!queue.empty()) {
    const auto [id, depth] = queue.front();
    queue.pop();
    if (!consume_visit(visited, request.limits.max_visited, depth, request.limits.max_depth)) {
      return finish_response(
        snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, true, false);
    }
    if (id != start->id) {
      if (const auto* node = provider.node(id); node != nullptr) {
        results.push_back(make_result("nodes", node->id, node_json(*node)));
        collect_evidence(provider, node->id, evidence);
      }
    }
    auto edges = provider.incoming(id);
    std::sort(edges.begin(), edges.end(), [](const SnapshotEdge* lhs, const SnapshotEdge* rhs) {
      return lhs->from < rhs->from;
    });
    for (const auto* edge : edges) {
      if (!kind_in(edge->kind, filter)) {
        continue;
      }
      if (seen.insert(edge->from).second) {
        queue.push({edge->from, depth + 1});
      }
    }
    if (results.size() >= request.limits.max_results) {
      return finish_response(
        snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, true, false);
    }
  }
  return finish_response(
    snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, false, false);
}

template <typename P>
QueryResponse path_query(
  const Snapshot& snapshot,
  std::string snapshot_id,
  P& provider,
  const QueryRequest& request
) {
  const auto* start = provider.node(request.subject);
  const auto* goal = provider.node(request.target);
  if (start == nullptr || goal == nullptr) {
    return finish_response(
      snapshot, std::move(snapshot_id), request, {}, {}, 1, false, true, true);
  }
  const auto filter = request.relation_kinds.empty()
    ? std::vector<std::string>{"Flow"}
    : request.relation_kinds;
  std::unordered_map<std::string, std::string> prev;
  std::unordered_set<std::string> seen;
  std::queue<std::pair<std::string, std::size_t>> queue;
  queue.push({start->id, 0});
  seen.insert(start->id);
  std::size_t visited = 0;
  bool found = false;
  while (!queue.empty()) {
    const auto [id, depth] = queue.front();
    queue.pop();
    if (!consume_visit(visited, request.limits.max_visited, depth, request.limits.max_depth)) {
      return finish_response(
        snapshot, std::move(snapshot_id), request, {}, {}, visited, true, false, true);
    }
    if (id == goal->id) {
      found = true;
      break;
    }
    std::vector<std::string> neighbors;
    for (const auto* edge : provider.outgoing(id)) {
      if (!kind_in(edge->kind, filter)) {
        continue;
      }
      neighbors.push_back(edge->to);
    }
    std::sort(neighbors.begin(), neighbors.end());
    neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
    for (const auto& next : neighbors) {
      if (seen.insert(next).second) {
        prev[next] = id;
        queue.push({next, depth + 1});
      }
    }
  }
  if (!found) {
    return finish_response(
      snapshot, std::move(snapshot_id), request, {}, {}, visited, false, false, true);
  }
  std::vector<std::string> path;
  for (std::string cursor = goal->id; ; cursor = prev[cursor]) {
    path.push_back(cursor);
    if (cursor == start->id) {
      break;
    }
  }
  std::reverse(path.begin(), path.end());
  std::vector<QueryResultRecord> results;
  std::vector<std::string> evidence;
  for (const auto& id : path) {
    if (const auto* node = provider.node(id); node != nullptr) {
      results.push_back(make_result("nodes", node->id, node_json(*node)));
      collect_evidence(provider, node->id, evidence);
    }
  }
  return finish_response(
    snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, false, false, true);
}

template <typename P>
QueryResponse lineage_query(
  const Snapshot& snapshot,
  std::string snapshot_id,
  P& provider,
  const QueryRequest& request
) {
  std::vector<QueryResultRecord> results;
  std::vector<std::string> evidence;
  std::size_t visited = 0;
  for (const auto* lineage : provider.lineage_candidates(request.subject)) {
    if (!consume_visit(visited, request.limits.max_visited, 0, request.limits.max_depth)) {
      return finish_response(
        snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, true, false);
    }
    results.push_back(make_result("lineage", lineage->id, lineage_json(*lineage)));
    evidence.insert(evidence.end(), lineage->evidence.begin(), lineage->evidence.end());
    if (results.size() >= request.limits.max_results) {
      return finish_response(
        snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, true, false);
    }
  }
  return finish_response(
    snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence), visited, false, false);
}

template <typename P>
QueryResponse why_query(
  const Snapshot& snapshot,
  std::string snapshot_id,
  P& provider,
  const QueryRequest& request
) {
  std::string start_evidence;
  if (const auto* node = provider.node(request.subject); node != nullptr) {
    start_evidence = node->evidence;
  } else if (const auto* edge = provider.edge(request.subject); edge != nullptr) {
    start_evidence = edge->evidence;
  } else if (const auto* fact = provider.fact(request.subject); fact != nullptr) {
    start_evidence = fact->evidence;
  } else if (const auto* evidence = provider.evidence(request.subject); evidence != nullptr) {
    start_evidence = evidence->id;
  } else {
    return finish_response(
      snapshot, std::move(snapshot_id), request, {}, {}, 1, false, true);
  }

  std::vector<QueryResultRecord> results;
  std::vector<std::string> evidence_ids;
  std::unordered_set<std::string> seen;
  std::queue<std::pair<std::string, std::size_t>> queue;
  queue.push({start_evidence, 0});
  std::size_t visited = 0;
  while (!queue.empty()) {
    const auto [id, depth] = queue.front();
    queue.pop();
    if (!seen.insert(id).second) {
      continue;
    }
    if (!consume_visit(visited, request.limits.max_visited, depth, request.limits.max_depth)
        || evidence_ids.size() >= request.limits.max_evidence) {
      return finish_response(
        snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence_ids), visited, true, false);
    }
    const auto* record = provider.evidence(id);
    if (record == nullptr) {
      continue;
    }
    CompactJson json;
    json.begin_object();
    json.key("ref");
    json.string_value(record->id);
    json.key("producer_rule");
    json.string_value(record->producer_rule);
    json.end_object();
    results.push_back(make_result("evidence", record->id, json.buf));
    evidence_ids.push_back(record->id);
    std::vector<std::string> prerequisites = record->prerequisites;
    std::sort(prerequisites.begin(), prerequisites.end());
    for (const auto& prerequisite : prerequisites) {
      queue.push({prerequisite, depth + 1});
    }
  }
  return finish_response(
    snapshot, std::move(snapshot_id), request, std::move(results), std::move(evidence_ids), visited, false, false);
}

template <typename P>
QueryResponse evaluate_with_provider(
  const Snapshot& snapshot,
  P& provider,
  QueryRequest request,
  std::string snapshot_id
) {
  request.limits = clamp_query_limits(request.limits);
  if (request.schema.major != kQuerySchemaMajor) {
    QueryResponse response;
    response.status = QueryStatus::Unsupported;
    response.reason = std::string(kReasonUnsupported);
    response.snapshot_id = snapshot_id;
    return response;
  }
  for (const auto& capability : request.required_capabilities) {
    if (!observable_capability_is_supported(capability)) {
      QueryResponse response;
      response.status = QueryStatus::Unsupported;
      response.reason = std::string(kReasonUnknownRequiredCapability);
      response.snapshot_id = snapshot_id;
      return response;
    }
  }

  QueryResponse response;
  switch (request.kind) {
    case QueryKind::Lookup:
      response = lookup_query(snapshot, std::move(snapshot_id), provider, request);
      break;
    case QueryKind::Dependencies:
      response = edge_query(snapshot, std::move(snapshot_id), provider, request, true);
      break;
    case QueryKind::Dependents:
      response = edge_query(snapshot, std::move(snapshot_id), provider, request, false);
      break;
    case QueryKind::Effects:
    case QueryKind::Ownership:
    case QueryKind::Mutation:
    case QueryKind::Failure:
      response = filtered_edge_query(snapshot, std::move(snapshot_id), provider, request);
      break;
    case QueryKind::TaskScope:
      response = scope_query(snapshot, std::move(snapshot_id), provider, request, false);
      break;
    case QueryKind::StreamScope:
      response = scope_query(snapshot, std::move(snapshot_id), provider, request, true);
      break;
    case QueryKind::Impact:
      response = impact_query(snapshot, std::move(snapshot_id), provider, request);
      break;
    case QueryKind::CanonicalPath:
      response = path_query(snapshot, std::move(snapshot_id), provider, request);
      break;
    case QueryKind::Lineage:
      response = lineage_query(snapshot, std::move(snapshot_id), provider, request);
      break;
    case QueryKind::Why:
      response = why_query(snapshot, std::move(snapshot_id), provider, request);
      break;
  }
  response.negotiated_capabilities = snapshot.capabilities;
  return response;
}

std::size_t shard_bytes(const IndexShard& shard) {
  std::size_t bytes = shard.subject_id.size();
  for (const auto& value : shard.dependency_keys) bytes += value.size();
  for (const auto& value : shard.outgoing_edges) bytes += value.size();
  for (const auto& value : shard.incoming_edges) bytes += value.size();
  for (const auto& value : shard.facts) bytes += value.size();
  for (const auto& value : shard.lineage) bytes += value.size();
  return bytes + 64;
}

// Subject membership maps built once per index so shard construction is
// O(nodes + edges + facts + lineage) instead of scanning every category per
// node.
struct SubjectMaps
{
  std::unordered_map<std::string, std::vector<std::string>> facts;
  std::unordered_map<std::string, std::vector<std::string>> lineage;
};

SubjectMaps build_subject_maps(const Snapshot& snapshot) {
  SubjectMaps maps;
  for (const auto& fact : snapshot.facts) {
    maps.facts[fact.subject].push_back(fact.id);
  }
  for (const auto& lineage : snapshot.lineage) {
    for (const auto& subject : lineage.prior) {
      maps.lineage[subject].push_back(lineage.id);
    }
    for (const auto& subject : lineage.target) {
      maps.lineage[subject].push_back(lineage.id);
    }
  }
  for (auto& [_, ids] : maps.facts) {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  }
  for (auto& [_, ids] : maps.lineage) {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  }
  return maps;
}

std::shared_ptr<IndexShard> make_shard(
  const SnapshotNode& node,
  const Adjacency& adj,
  const SubjectMaps& maps
) {
  auto shard = std::make_shared<IndexShard>();
  shard->subject_id = node.id;
  shard->dependency_keys.push_back(node.id);
  if (const auto it = adj.outgoing.find(node.id); it != adj.outgoing.end()) {
    for (const auto* edge : it->second) {
      shard->outgoing_edges.push_back(edge->id);
      shard->dependency_keys.push_back(edge->id);
    }
  }
  if (const auto it = adj.incoming.find(node.id); it != adj.incoming.end()) {
    for (const auto* edge : it->second) {
      shard->incoming_edges.push_back(edge->id);
      shard->dependency_keys.push_back(edge->id);
    }
  }
  if (const auto it = maps.facts.find(node.id); it != maps.facts.end()) {
    shard->facts = it->second;
    shard->dependency_keys.insert(
      shard->dependency_keys.end(), it->second.begin(), it->second.end());
  }
  if (const auto it = maps.lineage.find(node.id); it != maps.lineage.end()) {
    shard->lineage = it->second;
    shard->dependency_keys.insert(
      shard->dependency_keys.end(), it->second.begin(), it->second.end());
  }
  std::sort(shard->dependency_keys.begin(), shard->dependency_keys.end());
  shard->dependency_keys.erase(
    std::unique(shard->dependency_keys.begin(), shard->dependency_keys.end()),
    shard->dependency_keys.end());
  shard->byte_size = shard_bytes(*shard);
  return shard;
}

void index_reverse_deps(SnapshotIndex& index) {
  index.reverse_deps.clear();
  for (const auto& shard : index.shards) {
    for (const auto& key : shard->dependency_keys) {
      index.reverse_deps[key].push_back(shard->subject_id);
    }
  }
}

std::unordered_set<std::string> invalidation_closure(
  const SnapshotIndex& parent,
  const std::vector<std::string>& seeds
) {
  std::unordered_set<std::string> invalidated;
  std::queue<std::string> queue;
  for (const auto& seed : seeds) {
    queue.push(seed);
    invalidated.insert(seed);
  }
  while (!queue.empty()) {
    const std::string key = queue.front();
    queue.pop();
    const auto it = parent.reverse_deps.find(key);
    if (it == parent.reverse_deps.end()) {
      continue;
    }
    for (const auto& subject : it->second) {
      if (invalidated.insert(subject).second) {
        queue.push(subject);
      }
    }
  }
  return invalidated;
}

} // namespace

QueryLimits clamp_query_limits(QueryLimits limits) {
  if (limits.max_results == 0) limits.max_results = kQueryDefaultResults;
  if (limits.max_depth == 0) limits.max_depth = kQueryDefaultDepth;
  if (limits.max_visited == 0) limits.max_visited = kQueryDefaultVisited;
  if (limits.max_evidence == 0) limits.max_evidence = kQueryDefaultEvidence;
  limits.max_results = std::min(limits.max_results, kQueryHardResults);
  limits.max_depth = std::min(limits.max_depth, kQueryHardDepth);
  limits.max_visited = std::min(limits.max_visited, kQueryHardVisited);
  limits.max_evidence = std::min(limits.max_evidence, kQueryHardEvidence);
  return limits;
}

std::string_view query_kind_name(QueryKind kind) noexcept {
  switch (kind) {
    case QueryKind::Lookup: return "lookup";
    case QueryKind::Dependencies: return "dependencies";
    case QueryKind::Dependents: return "dependents";
    case QueryKind::Effects: return "effects";
    case QueryKind::Ownership: return "ownership";
    case QueryKind::Mutation: return "mutation";
    case QueryKind::Failure: return "failure";
    case QueryKind::TaskScope: return "task_scope";
    case QueryKind::StreamScope: return "stream_scope";
    case QueryKind::Impact: return "impact";
    case QueryKind::CanonicalPath: return "canonical_path";
    case QueryKind::Lineage: return "lineage";
    case QueryKind::Why: return "why";
  }
  return "unknown";
}

std::optional<QueryKind> parse_query_kind(std::string_view name) {
  if (name == "lookup") return QueryKind::Lookup;
  if (name == "dependencies") return QueryKind::Dependencies;
  if (name == "dependents") return QueryKind::Dependents;
  if (name == "effects") return QueryKind::Effects;
  if (name == "ownership") return QueryKind::Ownership;
  if (name == "mutation") return QueryKind::Mutation;
  if (name == "failure") return QueryKind::Failure;
  if (name == "task_scope") return QueryKind::TaskScope;
  if (name == "stream_scope") return QueryKind::StreamScope;
  if (name == "impact") return QueryKind::Impact;
  if (name == "canonical_path") return QueryKind::CanonicalPath;
  if (name == "lineage") return QueryKind::Lineage;
  if (name == "why") return QueryKind::Why;
  return std::nullopt;
}

std::string_view query_status_name(QueryStatus status) noexcept {
  switch (status) {
    case QueryStatus::Complete: return "complete";
    case QueryStatus::Partial: return "partial";
    case QueryStatus::Truncated: return "truncated";
    case QueryStatus::Unsupported: return "unsupported";
    case QueryStatus::Invalid: return "invalid";
  }
  return "invalid";
}

NegotiationResult negotiate_observable_contracts(const NegotiationRequest& request) {
  NegotiationResult result;
  if (request.snapshot_schema != kStaticSnapshotSchemaVersion) {
    result.reason = std::string(kReasonMajorIncompatible);
    return result;
  }
  auto negotiate_version = [](ContractVersion offered, int supported_major, int supported_minor, std::string& reason)
    -> std::optional<ContractVersion> {
    if (offered.major != supported_major) {
      reason = std::string(kReasonMajorIncompatible);
      return std::nullopt;
    }
    if (offered.minor < 0) {
      reason = std::string(kReasonUnsupported);
      return std::nullopt;
    }
    ContractVersion selected{supported_major, std::min(offered.minor, supported_minor)};
    if (selected.minor < 1 && supported_minor >= 1) {
      reason = std::string(kReasonUnsupported);
      return std::nullopt;
    }
    return selected;
  };

  std::string reason;
  const auto delta = negotiate_version(request.delta, kDeltaSchemaMajor, kDeltaSchemaMinor, reason);
  const auto query = negotiate_version(request.query, kQuerySchemaMajor, kQuerySchemaMinor, reason);
  const auto lineage = negotiate_version(request.lineage, 0, 1, reason);
  if (!delta || !query || !lineage) {
    result.reason = reason.empty() ? std::string(kReasonUnsupported) : reason;
    return result;
  }
  for (const auto& capability : request.required_capabilities) {
    if (!observable_capability_is_supported(capability)) {
      result.reason = std::string(kReasonUnknownRequiredCapability);
      return result;
    }
    result.capabilities.push_back(capability);
  }
  for (const auto& capability : request.optional_capabilities) {
    if (observable_capability_is_supported(capability)
        && std::find(result.capabilities.begin(), result.capabilities.end(), capability)
          == result.capabilities.end()) {
      result.capabilities.push_back(capability);
    }
  }
  std::sort(result.capabilities.begin(), result.capabilities.end());
  result.ok = true;
  result.snapshot_schema = kStaticSnapshotSchemaVersion;
  result.delta = *delta;
  result.query = *query;
  result.lineage = *lineage;
  return result;
}

QueryRequest parse_query_request(std::string_view json, std::string& error) {
  QueryRequest request;
  json_detail::JsonValue root;
  if (!json_detail::parse_json(json, root, error) || !root.is_object()) {
    error = error.empty() ? "query request must be an object" : error;
    return request;
  }
  const auto* contract = root.field("contract");
  if (contract != nullptr && contract->kind == json_detail::JsonValue::Kind::String) {
    request.contract = contract->string_value;
  }
  const auto kind = parse_query_kind(json_detail::require_string(root, "kind", error));
  if (!kind) {
    error = "unknown query kind";
    return request;
  }
  request.kind = *kind;
  request.subject = json_detail::require_string(root, "subject", error);
  const auto* target = root.field("target");
  if (target != nullptr && target->kind == json_detail::JsonValue::Kind::String) {
    request.target = target->string_value;
  }
  const auto* kinds = root.field("relation_kinds");
  if (kinds != nullptr) {
    request.relation_kinds = json_detail::require_string_array(root, "relation_kinds", error);
  }
  const auto* limits = root.field("limits");
  if (limits != nullptr && limits->is_object()) {
    auto read_limit = [&](std::string_view key, std::size_t& out) {
      const auto* field = limits->field(key);
      if (field != nullptr && field->kind == json_detail::JsonValue::Kind::Int && field->int_value >= 0) {
        out = static_cast<std::size_t>(field->int_value);
      }
    };
    read_limit("max_results", request.limits.max_results);
    read_limit("max_depth", request.limits.max_depth);
    read_limit("max_visited", request.limits.max_visited);
    read_limit("max_evidence", request.limits.max_evidence);
  }
  const auto* required = root.field("required_capabilities");
  if (required != nullptr) {
    request.required_capabilities = json_detail::require_string_array(root, "required_capabilities", error);
  }
  const auto* optional = root.field("optional_capabilities");
  if (optional != nullptr) {
    request.optional_capabilities = json_detail::require_string_array(root, "optional_capabilities", error);
  }
  return request;
}

std::string serialize_query_response(const QueryResponse& response) {
  CompactJson json;
  json.begin_object();
  json.key("snapshot_id");
  json.string_value(response.snapshot_id);
  json.key("status");
  json.string_value(query_status_name(response.status));
  json.key("completeness");
  json.string_value(response.completeness);
  json.key("reason");
  json.string_value(response.reason);
  json.key("results");
  json.begin_array();
  for (const auto& result : response.results) {
    json.begin_object();
    json.key("category");
    json.string_value(result.category);
    json.key("id");
    json.string_value(result.id);
    json.key("record");
    json.raw_value(result.json);
    json.end_object();
  }
  json.end_array();
  json.key("evidence");
  json.begin_array();
  for (const auto& evidence : response.evidence) {
    json.string_value(evidence);
  }
  json.end_array();
  json.key("visited");
  json.integer_value(static_cast<long long>(response.visited));
  json.key("evidence_count");
  json.integer_value(static_cast<long long>(response.evidence_count));
  json.key("truncated");
  json.bool_value(response.truncated);
  json.end_object();
  json.buf.push_back('\n');
  return json.buf;
}

QueryResponse evaluate_query_reference(const Snapshot& snapshot, const QueryRequest& request) {
  ReferenceProvider provider(snapshot);
  return evaluate_with_provider(snapshot, provider, request, snapshot_identity(snapshot));
}

SnapshotIndex build_snapshot_index(const Snapshot& snapshot) {
  SnapshotIndex index;
  index.snapshot_id = snapshot_identity(snapshot);
  const auto adj = build_adjacency(snapshot);
  const auto maps = build_subject_maps(snapshot);
  index.shards.reserve(snapshot.nodes.size());
  for (const auto& node : snapshot.nodes) {
    auto shard = make_shard(node, adj, maps);
    index.shard_by_subject[shard->subject_id] = index.shards.size();
    index.byte_size += shard->byte_size;
    index.shards.push_back(std::move(shard));
  }
  index.rebuilt_shards = index.shards.size();
  index_reverse_deps(index);
  return index;
}

SnapshotIndex merge_snapshot_index(
  const SnapshotIndex& parent_index,
  const Snapshot& child,
  const TopologyDelta& delta
) {
  SnapshotIndex index;
  index.snapshot_id = snapshot_identity(child);
  const auto adj = build_adjacency(child);
  const auto maps = build_subject_maps(child);
  auto seeds = delta_seed_keys(delta);
  // Added records never appear in the parent's reverse-dependency table, so
  // seed the subjects they attach to directly from the child snapshot.
  bool rebuild_all = false;
  for (const auto& op : delta.operations) {
    if (op.kind != DeltaOpKind::Add) {
      continue;
    }
    switch (op.category) {
      case RecordCategory::Edges: {
        if (const auto* edge = find_sorted(child.edges, op.key); edge != nullptr) {
          seeds.push_back(edge->from);
          seeds.push_back(edge->to);
        } else {
          rebuild_all = true;
        }
        break;
      }
      case RecordCategory::Facts: {
        if (const auto* fact = find_sorted(child.facts, op.key); fact != nullptr) {
          seeds.push_back(fact->subject);
        } else {
          rebuild_all = true;
        }
        break;
      }
      case RecordCategory::Lineage: {
        if (const auto* lineage = find_sorted(child.lineage, op.key); lineage != nullptr) {
          seeds.insert(seeds.end(), lineage->prior.begin(), lineage->prior.end());
          seeds.insert(seeds.end(), lineage->target.begin(), lineage->target.end());
        } else {
          rebuild_all = true;
        }
        break;
      }
      default:
        break;
    }
  }
  const auto invalidated = rebuild_all
    ? std::unordered_set<std::string>{}
    : invalidation_closure(parent_index, seeds);
  index.shards.reserve(child.nodes.size());
  for (const auto& node : child.nodes) {
    if (!rebuild_all && invalidated.count(node.id) == 0) {
      if (const auto it = parent_index.shard_by_subject.find(node.id);
          it != parent_index.shard_by_subject.end()) {
        const auto& prior = parent_index.shards[it->second];
        index.shard_by_subject[node.id] = index.shards.size();
        index.byte_size += prior->byte_size;
        index.shards.push_back(prior);
        ++index.reused_shards;
        continue;
      }
    }
    auto shard = make_shard(node, adj, maps);
    index.shard_by_subject[node.id] = index.shards.size();
    index.byte_size += shard->byte_size;
    index.shards.push_back(std::move(shard));
    ++index.rebuilt_shards;
  }
  index_reverse_deps(index);
  return index;
}

QueryResponse evaluate_query_index(
  const Snapshot& snapshot,
  const SnapshotIndex& index,
  const QueryRequest& request,
  QueryIndexProbe* probe
) {
  IndexProvider provider{snapshot, index, probe};
  return evaluate_with_provider(snapshot, provider, request, index.snapshot_id);
}

const IndexShard* find_shard(const SnapshotIndex& index, std::string_view subject) {
  const auto it = index.shard_by_subject.find(std::string(subject));
  if (it == index.shard_by_subject.end()) {
    return nullptr;
  }
  return index.shards[it->second].get();
}

} // namespace styio::observable
