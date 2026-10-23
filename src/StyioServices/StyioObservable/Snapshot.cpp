#include "Snapshot.hpp"

#include "JsonSupport.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace styio::observable {
namespace {

using json_detail::CompactJson;
using json_detail::JsonValue;

void emit_string_array(CompactJson& json, const std::vector<std::string>& values) {
  json.begin_array();
  for (const auto& value : values) {
    json.string_value(value);
  }
  json.end_array();
}

void emit_node(CompactJson& json, const SnapshotNode& node) {
  json.begin_object();
  json.key("id");
  json.string_value(node.id);
  json.key("kind");
  json.string_value(node.kind);
  json.key("role");
  json.string_value(node.role);
  json.key("anchors");
  emit_string_array(json, node.anchors);
  json.key("evidence");
  json.string_value(node.evidence);
  json.end_object();
}

void emit_edge(CompactJson& json, const SnapshotEdge& edge) {
  json.begin_object();
  json.key("id");
  json.string_value(edge.id);
  json.key("kind");
  json.string_value(edge.kind);
  json.key("from");
  json.string_value(edge.from);
  json.key("to");
  json.string_value(edge.to);
  json.key("evidence");
  json.string_value(edge.evidence);
  json.end_object();
}

void emit_fact(CompactJson& json, const SnapshotFact& fact) {
  json.begin_object();
  json.key("id");
  json.string_value(fact.id);
  json.key("subject");
  json.string_value(fact.subject);
  json.key("predicate");
  json.string_value(fact.predicate);
  json.key("value");
  if (fact.value_is_array) {
    emit_string_array(json, fact.array_value);
  } else {
    json.string_value(fact.string_value);
  }
  json.key("evidence");
  json.string_value(fact.evidence);
  json.end_object();
}

void emit_diagnostic(CompactJson& json, const SnapshotDiagnostic& diagnostic) {
  json.begin_object();
  json.key("id");
  json.string_value(diagnostic.id);
  json.key("code");
  json.string_value(diagnostic.code);
  json.key("severity");
  json.string_value(diagnostic.severity);
  json.key("subject");
  json.string_value(diagnostic.subject);
  json.key("evidence");
  json.string_value(diagnostic.evidence);
  json.end_object();
}

void emit_anchor(CompactJson& json, const SnapshotAnchor& anchor) {
  json.begin_object();
  json.key("ref");
  json.string_value(anchor.id);
  json.key("path");
  json.string_value(anchor.path);
  json.key("precision");
  json.string_value(anchor.precision);
  json.end_object();
}

void emit_evidence(CompactJson& json, const SnapshotEvidence& evidence) {
  json.begin_object();
  json.key("ref");
  json.string_value(evidence.id);
  json.key("producer_rule");
  json.string_value(evidence.producer_rule);
  json.key("rule_version");
  json.string_value(evidence.rule_version);
  json.key("subjects");
  emit_string_array(json, evidence.subjects);
  json.key("prerequisites");
  emit_string_array(json, evidence.prerequisites);
  json.key("anchors");
  emit_string_array(json, evidence.anchors);
  json.end_object();
}

void emit_lineage(CompactJson& json, const LineageRecord& lineage) {
  json.begin_object();
  json.key("id");
  json.string_value(lineage.id);
  json.key("kind");
  json.string_value(lineage.kind);
  json.key("prior");
  emit_string_array(json, lineage.prior);
  json.key("target");
  emit_string_array(json, lineage.target);
  json.key("producer_rule");
  json.string_value(lineage.producer_rule);
  json.key("rule_version");
  json.string_value(lineage.rule_version);
  json.key("evidence");
  emit_string_array(json, lineage.evidence);
  json.key("completeness");
  json.string_value(lineage.completeness);
  json.end_object();
}

bool insert_unique_id(
  std::unordered_set<std::string>& seen,
  const std::string& id,
  std::string& error
) {
  if (!seen.insert(id).second) {
    error = "observable static snapshot identity collision: " + id;
    return false;
  }
  return true;
}

bool evidence_is_acyclic(const std::vector<SnapshotEvidence>& evidence, std::string& error) {
  std::unordered_map<std::string, std::size_t> index;
  index.reserve(evidence.size());
  for (std::size_t i = 0; i < evidence.size(); ++i) {
    index.emplace(evidence[i].id, i);
  }
  std::vector<int> indegree(evidence.size(), 0);
  std::vector<std::vector<std::size_t>> adj(evidence.size());
  for (std::size_t i = 0; i < evidence.size(); ++i) {
    for (const auto& prerequisite : evidence[i].prerequisites) {
      const auto it = index.find(prerequisite);
      if (it == index.end()) {
        error = "observable static snapshot evidence prerequisite does not resolve: "
          + prerequisite;
        return false;
      }
      adj[it->second].push_back(i);
      indegree[i] += 1;
    }
  }
  std::vector<std::size_t> ready;
  ready.reserve(evidence.size());
  for (std::size_t i = 0; i < evidence.size(); ++i) {
    if (indegree[i] == 0) {
      ready.push_back(i);
    }
  }
  std::size_t seen = 0;
  for (std::size_t cursor = 0; cursor < ready.size(); ++cursor) {
    const std::size_t node = ready[cursor];
    ++seen;
    for (const std::size_t next : adj[node]) {
      indegree[next] -= 1;
      if (indegree[next] == 0) {
        ready.push_back(next);
      }
    }
  }
  if (seen != evidence.size()) {
    error = "observable static snapshot evidence graph contains a cycle";
    return false;
  }
  return true;
}

template <typename T>
const T* find_by_id(const std::vector<T>& records, std::string_view id) {
  for (const auto& record : records) {
    if (record.id == id) {
      return &record;
    }
  }
  return nullptr;
}

} // namespace

bool static_snapshot_capability_is_supported(std::string_view name) noexcept {
  for (const std::string_view capability : kStaticSnapshotCapabilities) {
    if (capability == name) {
      return true;
    }
  }
  return false;
}

bool observable_capability_is_supported(std::string_view name) noexcept {
  if (static_snapshot_capability_is_supported(name)) {
    return true;
  }
  for (const std::string_view capability : kS2OptionalCapabilities) {
    if (capability == name) {
      return true;
    }
  }
  return false;
}

std::string serialize_snapshot(const Snapshot& snapshot) {
  CompactJson json;
  json.begin_object();
  json.key("contract");
  json.string_value(snapshot.contract.empty() ? kStaticSnapshotContractName : snapshot.contract);
  json.key("schema_version");
  json.integer_value(snapshot.schema_version);
  json.key("stability");
  json.string_value(snapshot.stability.empty() ? kStaticSnapshotStability : snapshot.stability);
  json.key("producer");
  json.begin_object();
  json.key("name");
  json.string_value(snapshot.producer.name);
  json.key("version");
  json.string_value(snapshot.producer.version);
  json.end_object();
  json.key("capabilities");
  json.begin_array();
  if (snapshot.capabilities.empty()) {
    for (const std::string_view capability : kStaticSnapshotCapabilities) {
      json.string_value(capability);
    }
  } else {
    for (const auto& capability : snapshot.capabilities) {
      json.string_value(capability);
    }
  }
  json.end_array();
  json.key("compilation_unit");
  json.begin_object();
  json.key("package_name");
  json.string_value(snapshot.compilation_unit.package_name);
  json.key("manifest_path");
  json.string_value(snapshot.compilation_unit.manifest_path);
  json.key("entry_path");
  json.string_value(snapshot.compilation_unit.entry_path);
  json.end_object();
  json.key("completeness");
  json.string_value(snapshot.completeness);
  json.key("root");
  if (snapshot.root_is_null) {
    json.null_value();
  } else {
    json.string_value(snapshot.root);
  }

  json.key("nodes");
  json.begin_array();
  for (const auto& node : snapshot.nodes) {
    emit_node(json, node);
  }
  json.end_array();

  json.key("edges");
  json.begin_array();
  for (const auto& edge : snapshot.edges) {
    emit_edge(json, edge);
  }
  json.end_array();

  json.key("facts");
  json.begin_array();
  for (const auto& fact : snapshot.facts) {
    emit_fact(json, fact);
  }
  json.end_array();

  json.key("anchors");
  json.begin_array();
  for (const auto& anchor : snapshot.anchors) {
    emit_anchor(json, anchor);
  }
  json.end_array();

  json.key("evidence");
  json.begin_array();
  for (const auto& evidence : snapshot.evidence) {
    emit_evidence(json, evidence);
  }
  json.end_array();

  if (!snapshot.diagnostics.empty()) {
    json.key("diagnostics");
    json.begin_array();
    for (const auto& diagnostic : snapshot.diagnostics) {
      emit_diagnostic(json, diagnostic);
    }
    json.end_array();
  }
  if (!snapshot.lineage.empty()) {
    json.key("lineage");
    json.begin_array();
    for (const auto& lineage : snapshot.lineage) {
      emit_lineage(json, lineage);
    }
    json.end_array();
  }
  if (!snapshot.parent_snapshot_id.empty()) {
    json.key("parent_snapshot_id");
    json.string_value(snapshot.parent_snapshot_id);
  }

  json.end_object();
  json.buf.push_back('\n');
  return json.buf;
}

std::string snapshot_identity(std::string_view canonical_json) {
  return std::string(kSnapshotIdPrefix) + json_detail::sha256_hex16(canonical_json);
}

std::string snapshot_identity(const Snapshot& snapshot) {
  return snapshot_identity(serialize_snapshot(snapshot));
}

SnapshotIssue finalize_snapshot(Snapshot snapshot) {
  SnapshotIssue result;
  if (snapshot.contract.empty()) {
    snapshot.contract = std::string(kStaticSnapshotContractName);
  }
  if (snapshot.stability.empty()) {
    snapshot.stability = std::string(kStaticSnapshotStability);
  }
  if (snapshot.schema_version == 0) {
    snapshot.schema_version = kStaticSnapshotSchemaVersion;
  }
  if (snapshot.capabilities.empty()) {
    for (const std::string_view capability : kStaticSnapshotCapabilities) {
      snapshot.capabilities.emplace_back(capability);
    }
  }

  auto by_id = [](const auto& lhs, const auto& rhs) { return lhs.id < rhs.id; };
  std::sort(snapshot.nodes.begin(), snapshot.nodes.end(), by_id);
  std::sort(snapshot.edges.begin(), snapshot.edges.end(), by_id);
  std::sort(snapshot.facts.begin(), snapshot.facts.end(), by_id);
  std::sort(snapshot.diagnostics.begin(), snapshot.diagnostics.end(), by_id);
  std::sort(snapshot.anchors.begin(), snapshot.anchors.end(), by_id);
  std::sort(snapshot.evidence.begin(), snapshot.evidence.end(), by_id);
  std::sort(snapshot.lineage.begin(), snapshot.lineage.end(), by_id);

  std::unordered_set<std::string> node_ids;
  std::unordered_set<std::string> edge_ids;
  std::unordered_set<std::string> fact_ids;
  std::unordered_set<std::string> diagnostic_ids;
  std::unordered_set<std::string> anchor_ids;
  std::unordered_set<std::string> evidence_ids;
  std::unordered_set<std::string> lineage_ids;
  std::unordered_set<std::string> all_ids;
  node_ids.reserve(snapshot.nodes.size());
  edge_ids.reserve(snapshot.edges.size());
  fact_ids.reserve(snapshot.facts.size());
  diagnostic_ids.reserve(snapshot.diagnostics.size());
  anchor_ids.reserve(snapshot.anchors.size());
  evidence_ids.reserve(snapshot.evidence.size());
  lineage_ids.reserve(snapshot.lineage.size());
  all_ids.reserve(
    snapshot.nodes.size() + snapshot.edges.size() + snapshot.facts.size()
    + snapshot.diagnostics.size() + snapshot.anchors.size()
    + snapshot.evidence.size() + snapshot.lineage.size());

  std::string error;
  for (const auto& node : snapshot.nodes) {
    if (!insert_unique_id(node_ids, node.id, error) || !insert_unique_id(all_ids, node.id, error)) {
      result.error = std::move(error);
      return result;
    }
  }
  for (const auto& edge : snapshot.edges) {
    if (!insert_unique_id(edge_ids, edge.id, error) || !insert_unique_id(all_ids, edge.id, error)) {
      result.error = std::move(error);
      return result;
    }
    if (!node_ids.count(edge.from) || !node_ids.count(edge.to)) {
      result.error = "observable static snapshot edge endpoint does not resolve";
      return result;
    }
  }
  for (const auto& fact : snapshot.facts) {
    if (!insert_unique_id(fact_ids, fact.id, error) || !insert_unique_id(all_ids, fact.id, error)) {
      result.error = std::move(error);
      return result;
    }
    if (!node_ids.count(fact.subject)) {
      result.error = "observable static snapshot fact subject does not resolve";
      return result;
    }
  }
  for (const auto& diagnostic : snapshot.diagnostics) {
    if (!insert_unique_id(diagnostic_ids, diagnostic.id, error)
        || !insert_unique_id(all_ids, diagnostic.id, error)) {
      result.error = std::move(error);
      return result;
    }
    if (!diagnostic.subject.empty() && !record_exists(snapshot, diagnostic.subject)
        && !node_ids.count(diagnostic.subject)) {
      result.error = "observable static snapshot diagnostic subject does not resolve";
      return result;
    }
  }
  for (const auto& anchor : snapshot.anchors) {
    if (!insert_unique_id(anchor_ids, anchor.id, error)
        || !insert_unique_id(all_ids, anchor.id, error)) {
      result.error = std::move(error);
      return result;
    }
  }
  for (const auto& evidence : snapshot.evidence) {
    if (!insert_unique_id(evidence_ids, evidence.id, error)
        || !insert_unique_id(all_ids, evidence.id, error)) {
      result.error = std::move(error);
      return result;
    }
    for (const auto& subject : evidence.subjects) {
      if (!node_ids.count(subject) && !edge_ids.count(subject) && !fact_ids.count(subject)
          && !diagnostic_ids.count(subject) && !record_exists(snapshot, subject)) {
        result.error = "observable static snapshot evidence subject does not resolve";
        return result;
      }
    }
    for (const auto& anchor : evidence.anchors) {
      if (!anchor_ids.count(anchor)) {
        result.error = "observable static snapshot evidence anchor does not resolve";
        return result;
      }
    }
  }
  for (const auto& lineage : snapshot.lineage) {
    if (!insert_unique_id(lineage_ids, lineage.id, error)
        || !insert_unique_id(all_ids, lineage.id, error)) {
      result.error = std::move(error);
      return result;
    }
    for (const auto& evidence : lineage.evidence) {
      if (!evidence_ids.count(evidence)) {
        result.error = "observable lineage evidence does not resolve";
        return result;
      }
    }
  }
  if (!snapshot.root_is_null && !node_ids.count(snapshot.root)) {
    result.error = "observable static snapshot root does not resolve";
    return result;
  }
  if (!evidence_is_acyclic(snapshot.evidence, error)) {
    result.error = std::move(error);
    return result;
  }

  result.json = serialize_snapshot(snapshot);
  result.snapshot_id = snapshot_identity(result.json);
  result.ok = true;
  result.snapshot = std::move(snapshot);
  result.counts.nodes = result.snapshot.nodes.size();
  result.counts.edges = result.snapshot.edges.size();
  result.counts.facts = result.snapshot.facts.size();
  result.counts.diagnostics = result.snapshot.diagnostics.size();
  result.counts.anchors = result.snapshot.anchors.size();
  result.counts.evidence = result.snapshot.evidence.size();
  result.counts.lineage = result.snapshot.lineage.size();
  result.counts.serialized_bytes = result.json.size();
  return result;
}

SnapshotIssue parse_snapshot(std::string_view json) {
  SnapshotIssue result;
  JsonValue root;
  std::string error;
  if (!json_detail::parse_json(json, root, error) || !root.is_object()) {
    result.error = error.empty() ? "snapshot JSON must be an object" : error;
    return result;
  }

  Snapshot snapshot;
  snapshot.contract = json_detail::require_string(root, "contract", error);
  const JsonValue* schema = root.field("schema_version");
  if (schema == nullptr || schema->kind != JsonValue::Kind::Int) {
    result.error = "snapshot schema_version must be an integer";
    return result;
  }
  snapshot.schema_version = static_cast<int>(schema->int_value);
  snapshot.stability = json_detail::require_string(root, "stability", error);
  const JsonValue* producer = root.field("producer");
  if (producer == nullptr || !producer->is_object()) {
    result.error = "snapshot producer must be an object";
    return result;
  }
  snapshot.producer.name = json_detail::require_string(*producer, "name", error);
  snapshot.producer.version = json_detail::require_string(*producer, "version", error);
  snapshot.capabilities = json_detail::require_string_array(root, "capabilities", error);
  const JsonValue* unit = root.field("compilation_unit");
  if (unit == nullptr || !unit->is_object()) {
    result.error = "snapshot compilation_unit must be an object";
    return result;
  }
  snapshot.compilation_unit.package_name = json_detail::require_string(*unit, "package_name", error);
  snapshot.compilation_unit.manifest_path = json_detail::require_string(*unit, "manifest_path", error);
  snapshot.compilation_unit.entry_path = json_detail::require_string(*unit, "entry_path", error);
  snapshot.completeness = json_detail::require_string(root, "completeness", error);
  const JsonValue* root_field = root.field("root");
  if (root_field == nullptr) {
    result.error = "snapshot root is missing";
    return result;
  }
  if (root_field->is_null()) {
    snapshot.root_is_null = true;
  } else if (root_field->kind == JsonValue::Kind::String) {
    snapshot.root = root_field->string_value;
  } else {
    result.error = "snapshot root must be a string or null";
    return result;
  }
  if (!error.empty()) {
    result.error = error;
    return result;
  }

  const JsonValue* nodes = root.field("nodes");
  const JsonValue* edges = root.field("edges");
  const JsonValue* facts = root.field("facts");
  const JsonValue* anchors = root.field("anchors");
  const JsonValue* evidence = root.field("evidence");
  if (nodes == nullptr || !nodes->is_array() || edges == nullptr || !edges->is_array()
      || facts == nullptr || !facts->is_array() || anchors == nullptr || !anchors->is_array()
      || evidence == nullptr || !evidence->is_array()) {
    result.error = "snapshot collections must be arrays";
    return result;
  }
  for (const auto& item : nodes->array_value) {
    if (!item.is_object()) {
      result.error = "node record must be an object";
      return result;
    }
    SnapshotNode node;
    node.id = json_detail::require_string(item, "id", error);
    node.kind = json_detail::require_string(item, "kind", error);
    node.role = json_detail::require_string(item, "role", error);
    node.anchors = json_detail::require_string_array(item, "anchors", error);
    node.evidence = json_detail::require_string(item, "evidence", error);
    snapshot.nodes.push_back(std::move(node));
  }
  for (const auto& item : edges->array_value) {
    if (!item.is_object()) {
      result.error = "edge record must be an object";
      return result;
    }
    SnapshotEdge edge;
    edge.id = json_detail::require_string(item, "id", error);
    edge.kind = json_detail::require_string(item, "kind", error);
    edge.from = json_detail::require_string(item, "from", error);
    edge.to = json_detail::require_string(item, "to", error);
    edge.evidence = json_detail::require_string(item, "evidence", error);
    snapshot.edges.push_back(std::move(edge));
  }
  for (const auto& item : facts->array_value) {
    if (!item.is_object()) {
      result.error = "fact record must be an object";
      return result;
    }
    SnapshotFact fact;
    fact.id = json_detail::require_string(item, "id", error);
    fact.subject = json_detail::require_string(item, "subject", error);
    fact.predicate = json_detail::require_string(item, "predicate", error);
    const JsonValue* value = item.field("value");
    if (value == nullptr) {
      result.error = "fact value is missing";
      return result;
    }
    if (value->kind == JsonValue::Kind::Array) {
      fact.value_is_array = true;
      for (const auto& entry : value->array_value) {
        if (entry.kind != JsonValue::Kind::String) {
          result.error = "fact array value must contain strings";
          return result;
        }
        fact.array_value.push_back(entry.string_value);
      }
    } else if (value->kind == JsonValue::Kind::String) {
      fact.string_value = value->string_value;
    } else {
      result.error = "fact value must be a string or string array";
      return result;
    }
    fact.evidence = json_detail::require_string(item, "evidence", error);
    snapshot.facts.push_back(std::move(fact));
  }
  for (const auto& item : anchors->array_value) {
    if (!item.is_object()) {
      result.error = "anchor record must be an object";
      return result;
    }
    SnapshotAnchor anchor;
    anchor.id = json_detail::require_string(item, "ref", error);
    anchor.path = json_detail::require_string(item, "path", error);
    anchor.precision = json_detail::require_string(item, "precision", error);
    snapshot.anchors.push_back(std::move(anchor));
  }
  for (const auto& item : evidence->array_value) {
    if (!item.is_object()) {
      result.error = "evidence record must be an object";
      return result;
    }
    SnapshotEvidence record;
    record.id = json_detail::require_string(item, "ref", error);
    record.producer_rule = json_detail::require_string(item, "producer_rule", error);
    record.rule_version = json_detail::require_string(item, "rule_version", error);
    record.subjects = json_detail::require_string_array(item, "subjects", error);
    record.prerequisites = json_detail::require_string_array(item, "prerequisites", error);
    record.anchors = json_detail::require_string_array(item, "anchors", error);
    snapshot.evidence.push_back(std::move(record));
  }

  const JsonValue* diagnostics = root.field("diagnostics");
  if (diagnostics != nullptr) {
    if (!diagnostics->is_array()) {
      result.error = "diagnostics must be an array";
      return result;
    }
    for (const auto& item : diagnostics->array_value) {
      if (!item.is_object()) {
        result.error = "diagnostic record must be an object";
        return result;
      }
      SnapshotDiagnostic diagnostic;
      diagnostic.id = json_detail::require_string(item, "id", error);
      diagnostic.code = json_detail::require_string(item, "code", error);
      diagnostic.severity = json_detail::require_string(item, "severity", error);
      diagnostic.subject = json_detail::require_string(item, "subject", error);
      diagnostic.evidence = json_detail::require_string(item, "evidence", error);
      snapshot.diagnostics.push_back(std::move(diagnostic));
    }
  }
  const JsonValue* lineage = root.field("lineage");
  if (lineage != nullptr) {
    if (!lineage->is_array()) {
      result.error = "lineage must be an array";
      return result;
    }
    for (const auto& item : lineage->array_value) {
      if (!item.is_object()) {
        result.error = "lineage record must be an object";
        return result;
      }
      LineageRecord record;
      record.id = json_detail::require_string(item, "id", error);
      record.kind = json_detail::require_string(item, "kind", error);
      record.prior = json_detail::require_string_array(item, "prior", error);
      record.target = json_detail::require_string_array(item, "target", error);
      record.producer_rule = json_detail::require_string(item, "producer_rule", error);
      record.rule_version = json_detail::require_string(item, "rule_version", error);
      record.evidence = json_detail::require_string_array(item, "evidence", error);
      record.completeness = json_detail::require_string(item, "completeness", error);
      snapshot.lineage.push_back(std::move(record));
    }
  }
  const JsonValue* parent = root.field("parent_snapshot_id");
  if (parent != nullptr) {
    if (parent->kind != JsonValue::Kind::String) {
      result.error = "parent_snapshot_id must be a string";
      return result;
    }
    snapshot.parent_snapshot_id = parent->string_value;
  }
  if (!error.empty()) {
    result.error = error;
    return result;
  }
  return finalize_snapshot(std::move(snapshot));
}

bool snapshot_topology_is_conclusive(const Snapshot& snapshot) noexcept {
  return snapshot.completeness == kCompletenessValidatedTopology;
}

std::string qualified_scope_key(const Snapshot& snapshot) {
  return snapshot.compilation_unit.package_name + '\n'
    + snapshot.compilation_unit.manifest_path + '\n'
    + snapshot.compilation_unit.entry_path;
}

const SnapshotNode* find_node(const Snapshot& snapshot, std::string_view id) {
  return find_by_id(snapshot.nodes, id);
}

const SnapshotEdge* find_edge(const Snapshot& snapshot, std::string_view id) {
  return find_by_id(snapshot.edges, id);
}

const SnapshotFact* find_fact(const Snapshot& snapshot, std::string_view id) {
  return find_by_id(snapshot.facts, id);
}

const SnapshotDiagnostic* find_diagnostic(const Snapshot& snapshot, std::string_view id) {
  return find_by_id(snapshot.diagnostics, id);
}

const SnapshotAnchor* find_anchor(const Snapshot& snapshot, std::string_view id) {
  return find_by_id(snapshot.anchors, id);
}

const SnapshotEvidence* find_evidence(const Snapshot& snapshot, std::string_view id) {
  return find_by_id(snapshot.evidence, id);
}

const LineageRecord* find_lineage(const Snapshot& snapshot, std::string_view id) {
  return find_by_id(snapshot.lineage, id);
}

bool record_exists(const Snapshot& snapshot, std::string_view id) {
  return find_node(snapshot, id) != nullptr
    || find_edge(snapshot, id) != nullptr
    || find_fact(snapshot, id) != nullptr
    || find_diagnostic(snapshot, id) != nullptr
    || find_anchor(snapshot, id) != nullptr
    || find_evidence(snapshot, id) != nullptr
    || find_lineage(snapshot, id) != nullptr;
}

} // namespace styio::observable
