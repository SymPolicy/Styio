#include "StaticSnapshotContract.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SHA256.h"

namespace styio::observable {
namespace {

using styio::resource_topology::EdgeKind;
using styio::resource_topology::NodeKind;
using styio::resource_topology::SemanticRole;
using styio::resource_topology::TypeState;
using styio::resource_topology::ValidatedArtifact;
using styio::semantic_identity::SemanticIdentity;

void append_u32(std::string& out, std::size_t value) {
  if (value > std::numeric_limits<std::uint32_t>::max()) {
    throw std::length_error("snapshot identity field exceeds supported size");
  }
  const auto encoded = static_cast<std::uint32_t>(value);
  for (int shift = 24; shift >= 0; shift -= 8) {
    out.push_back(static_cast<char>((encoded >> shift) & 0xffu));
  }
}

void append_field(std::string& out, std::string_view value) {
  append_u32(out, value.size());
  out.append(value.data(), value.size());
}

void append_bytes(std::string& out, const SemanticIdentity& identity) {
  append_u32(out, identity.bytes.size());
  out.append(reinterpret_cast<const char*>(identity.bytes.data()), identity.bytes.size());
}

SemanticIdentity hash_preimage(std::string_view preimage) {
  llvm::SHA256 hasher;
  hasher.update(llvm::StringRef(preimage.data(), preimage.size()));
  const auto digest = hasher.final();
  SemanticIdentity identity;
  std::copy_n(digest.begin(), identity.bytes.size(), identity.bytes.begin());
  return identity;
}

std::string prefixed_id(std::string_view prefix, const SemanticIdentity& identity) {
  std::string out;
  out.reserve(prefix.size() + identity.bytes.size() * 2);
  out.append(prefix.data(), prefix.size());
  out.append(styio::semantic_identity::encode_hex(identity));
  return out;
}

struct CompactJson
{
  std::string buf;
  std::vector<char> need_comma;

  void comma() {
    if (!need_comma.empty() && need_comma.back() != 0) {
      buf.push_back(',');
    }
    if (!need_comma.empty()) {
      need_comma.back() = 1;
    }
  }

  void begin_object() {
    comma();
    buf.push_back('{');
    need_comma.push_back(0);
  }

  void end_object() {
    buf.push_back('}');
    need_comma.pop_back();
  }

  void begin_array() {
    comma();
    buf.push_back('[');
    need_comma.push_back(0);
  }

  void end_array() {
    buf.push_back(']');
    need_comma.pop_back();
  }

  void key(std::string_view name) {
    comma();
    emit_string(name);
    buf.push_back(':');
    if (!need_comma.empty()) {
      need_comma.back() = 0;
    }
  }

  void emit_string(std::string_view value) {
    buf.push_back('"');
    for (unsigned char ch : value) {
      switch (ch) {
        case '"': buf += "\\\""; break;
        case '\\': buf += "\\\\"; break;
        case '\b': buf += "\\b"; break;
        case '\f': buf += "\\f"; break;
        case '\n': buf += "\\n"; break;
        case '\r': buf += "\\r"; break;
        case '\t': buf += "\\t"; break;
        default:
          if (ch < 0x20) {
            static constexpr char kHex[] = "0123456789abcdef";
            buf += "\\u00";
            buf.push_back(kHex[ch >> 4]);
            buf.push_back(kHex[ch & 0x0fu]);
          } else {
            buf.push_back(static_cast<char>(ch));
          }
          break;
      }
    }
    buf.push_back('"');
  }

  void string_value(std::string_view value) {
    comma();
    emit_string(value);
  }

  void null_value() {
    comma();
    buf += "null";
  }

  void integer_value(int value) {
    comma();
    buf += std::to_string(value);
  }
};

struct EvidenceRecord
{
  std::string id;
  std::string producer_rule;
  std::string rule_version;
  std::vector<std::string> subjects;
  std::vector<std::string> prerequisites;
  std::vector<std::string> anchors;
};

struct NodeRecord
{
  std::string id;
  std::string kind;
  std::string role;
  std::vector<std::string> anchors;
  std::string evidence;
};

struct EdgeRecord
{
  std::string id;
  std::string kind;
  std::string from;
  std::string to;
  std::string evidence;
};

struct FactRecord
{
  std::string id;
  std::string subject;
  std::string predicate;
  bool value_is_array = false;
  std::vector<std::string> array_value;
  std::string string_value;
  std::string evidence;
};

struct AnchorRecord
{
  std::string id;
  std::string path;
  std::string precision;
};

struct SnapshotModel
{
  styio::config::CompilationUnit unit;
  SnapshotProducer producer;
  std::string completeness;
  std::string root;
  bool root_is_null = false;
  std::vector<NodeRecord> nodes;
  std::vector<EdgeRecord> edges;
  std::vector<FactRecord> facts;
  std::vector<AnchorRecord> anchors;
  std::vector<EvidenceRecord> evidence;
};

std::string derive_snapshot_id(
  std::string_view prefix,
  std::string_view domain,
  const std::vector<std::string>& fields
) {
  std::string preimage;
  append_field(preimage, domain);
  append_u32(preimage, fields.size());
  for (const auto& field : fields) {
    append_field(preimage, field);
  }
  return prefixed_id(prefix, hash_preimage(preimage));
}

std::string node_evidence_rule(SemanticRole role) {
  return std::string("styio.sema.topology.node.") + styio::resource_topology::to_string(role);
}

std::string edge_evidence_rule(EdgeKind kind, std::string_view relation_key) {
  std::string rule = std::string("styio.sema.topology.relation.")
    + styio::resource_topology::to_string(kind) + ".";
  rule.append(relation_key.empty() ? "default" : relation_key);
  return rule;
}

std::string fact_evidence_rule(std::string_view predicate) {
  return std::string("styio.observable.static.fact.normalize.") + std::string(predicate);
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

bool evidence_is_acyclic(const std::vector<EvidenceRecord>& evidence, std::string& error) {
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

std::string serialize_snapshot(const SnapshotModel& model) {
  CompactJson json;
  json.begin_object();
  json.key("contract");
  json.string_value(kStaticSnapshotContractName);
  json.key("schema_version");
  json.integer_value(kStaticSnapshotSchemaVersion);
  json.key("stability");
  json.string_value(kStaticSnapshotStability);
  json.key("producer");
  json.begin_object();
  json.key("name");
  json.string_value(model.producer.name);
  json.key("version");
  json.string_value(model.producer.version);
  json.end_object();
  json.key("capabilities");
  json.begin_array();
  for (const std::string_view capability : kStaticSnapshotCapabilities) {
    json.string_value(capability);
  }
  json.end_array();
  json.key("compilation_unit");
  json.begin_object();
  json.key("package_name");
  json.string_value(model.unit.package_name);
  json.key("manifest_path");
  json.string_value(model.unit.manifest_relative_path);
  json.key("entry_path");
  json.string_value(model.unit.entry_relative_path);
  json.end_object();
  json.key("completeness");
  json.string_value(model.completeness);
  json.key("root");
  if (model.root_is_null) {
    json.null_value();
  } else {
    json.string_value(model.root);
  }

  json.key("nodes");
  json.begin_array();
  for (const auto& node : model.nodes) {
    json.begin_object();
    json.key("id");
    json.string_value(node.id);
    json.key("kind");
    json.string_value(node.kind);
    json.key("role");
    json.string_value(node.role);
    json.key("anchors");
    json.begin_array();
    for (const auto& anchor : node.anchors) {
      json.string_value(anchor);
    }
    json.end_array();
    json.key("evidence");
    json.string_value(node.evidence);
    json.end_object();
  }
  json.end_array();

  json.key("edges");
  json.begin_array();
  for (const auto& edge : model.edges) {
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
  json.end_array();

  json.key("facts");
  json.begin_array();
  for (const auto& fact : model.facts) {
    json.begin_object();
    json.key("id");
    json.string_value(fact.id);
    json.key("subject");
    json.string_value(fact.subject);
    json.key("predicate");
    json.string_value(fact.predicate);
    json.key("value");
    if (fact.value_is_array) {
      json.begin_array();
      for (const auto& item : fact.array_value) {
        json.string_value(item);
      }
      json.end_array();
    } else {
      json.string_value(fact.string_value);
    }
    json.key("evidence");
    json.string_value(fact.evidence);
    json.end_object();
  }
  json.end_array();

  json.key("anchors");
  json.begin_array();
  for (const auto& anchor : model.anchors) {
    json.begin_object();
    json.key("ref");
    json.string_value(anchor.id);
    json.key("path");
    json.string_value(anchor.path);
    json.key("precision");
    json.string_value(anchor.precision);
    json.end_object();
  }
  json.end_array();

  json.key("evidence");
  json.begin_array();
  for (const auto& evidence : model.evidence) {
    json.begin_object();
    json.key("ref");
    json.string_value(evidence.id);
    json.key("producer_rule");
    json.string_value(evidence.producer_rule);
    json.key("rule_version");
    json.string_value(evidence.rule_version);
    json.key("subjects");
    json.begin_array();
    for (const auto& subject : evidence.subjects) {
      json.string_value(subject);
    }
    json.end_array();
    json.key("prerequisites");
    json.begin_array();
    for (const auto& prerequisite : evidence.prerequisites) {
      json.string_value(prerequisite);
    }
    json.end_array();
    json.key("anchors");
    json.begin_array();
    for (const auto& anchor : evidence.anchors) {
      json.string_value(anchor);
    }
    json.end_array();
    json.end_object();
  }
  json.end_array();
  json.end_object();
  json.buf.push_back('\n');
  return json.buf;
}

SnapshotPublishResult finish_model(SnapshotModel model, std::string error_if_any) {
  SnapshotPublishResult result;
  if (!error_if_any.empty()) {
    result.error = std::move(error_if_any);
    return result;
  }

  auto by_id = [](const auto& lhs, const auto& rhs) { return lhs.id < rhs.id; };
  std::sort(model.nodes.begin(), model.nodes.end(), by_id);
  std::sort(model.edges.begin(), model.edges.end(), by_id);
  std::sort(model.facts.begin(), model.facts.end(), by_id);
  std::sort(model.anchors.begin(), model.anchors.end(), by_id);
  std::sort(model.evidence.begin(), model.evidence.end(), by_id);

  std::unordered_set<std::string> node_ids;
  std::unordered_set<std::string> edge_ids;
  std::unordered_set<std::string> fact_ids;
  std::unordered_set<std::string> anchor_ids;
  std::unordered_set<std::string> evidence_ids;
  std::unordered_set<std::string> all_ids;
  node_ids.reserve(model.nodes.size());
  edge_ids.reserve(model.edges.size());
  fact_ids.reserve(model.facts.size());
  anchor_ids.reserve(model.anchors.size());
  evidence_ids.reserve(model.evidence.size());
  all_ids.reserve(
    model.nodes.size() + model.edges.size() + model.facts.size()
    + model.anchors.size() + model.evidence.size());

  std::string error;
  for (const auto& node : model.nodes) {
    if (!insert_unique_id(node_ids, node.id, error) || !insert_unique_id(all_ids, node.id, error)) {
      result.error = std::move(error);
      return result;
    }
  }
  for (const auto& edge : model.edges) {
    if (!insert_unique_id(edge_ids, edge.id, error) || !insert_unique_id(all_ids, edge.id, error)) {
      result.error = std::move(error);
      return result;
    }
    if (!node_ids.count(edge.from) || !node_ids.count(edge.to)) {
      result.error = "observable static snapshot edge endpoint does not resolve";
      return result;
    }
  }
  for (const auto& fact : model.facts) {
    if (!insert_unique_id(fact_ids, fact.id, error) || !insert_unique_id(all_ids, fact.id, error)) {
      result.error = std::move(error);
      return result;
    }
    if (!node_ids.count(fact.subject)) {
      result.error = "observable static snapshot fact subject does not resolve";
      return result;
    }
  }
  for (const auto& anchor : model.anchors) {
    if (!insert_unique_id(anchor_ids, anchor.id, error)
        || !insert_unique_id(all_ids, anchor.id, error)) {
      result.error = std::move(error);
      return result;
    }
  }
  for (const auto& evidence : model.evidence) {
    if (!insert_unique_id(evidence_ids, evidence.id, error)
        || !insert_unique_id(all_ids, evidence.id, error)) {
      result.error = std::move(error);
      return result;
    }
    for (const auto& subject : evidence.subjects) {
      if (!node_ids.count(subject) && !edge_ids.count(subject) && !fact_ids.count(subject)) {
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
  if (!model.root_is_null && !node_ids.count(model.root)) {
    result.error = "observable static snapshot root does not resolve";
    return result;
  }
  if (!evidence_is_acyclic(model.evidence, error)) {
    result.error = std::move(error);
    return result;
  }

  result.json = serialize_snapshot(model);
  result.ok = true;
  result.counts.nodes = model.nodes.size();
  result.counts.edges = model.edges.size();
  result.counts.facts = model.facts.size();
  result.counts.anchors = model.anchors.size();
  result.counts.evidence = model.evidence.size();
  result.counts.serialized_bytes = result.json.size();
  return result;
}

EvidenceRecord make_evidence(
  const styio::config::CompilationUnit& unit,
  std::string producer_rule,
  std::vector<std::string> subjects,
  std::vector<std::string> prerequisites,
  std::vector<std::string> anchors
) {
  std::sort(subjects.begin(), subjects.end());
  subjects.erase(std::unique(subjects.begin(), subjects.end()), subjects.end());
  std::sort(prerequisites.begin(), prerequisites.end());
  prerequisites.erase(
    std::unique(prerequisites.begin(), prerequisites.end()), prerequisites.end());
  std::sort(anchors.begin(), anchors.end());
  anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());

  std::vector<std::string> identity_fields{
    unit.package_name,
    unit.manifest_relative_path,
    unit.entry_relative_path,
    producer_rule,
    "1",
  };
  identity_fields.insert(identity_fields.end(), subjects.begin(), subjects.end());
  identity_fields.insert(identity_fields.end(), prerequisites.begin(), prerequisites.end());
  identity_fields.insert(identity_fields.end(), anchors.begin(), anchors.end());
  std::string id = derive_snapshot_id(
    kEvidenceIdPrefix,
    "styio.observable-static-snapshot.evidence.v1",
    identity_fields);

  return EvidenceRecord{
    std::move(id),
    std::move(producer_rule),
    "1",
    std::move(subjects),
    std::move(prerequisites),
    std::move(anchors)};
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

SnapshotPublishResult publish_proven_scalar_noop(
  const styio::config::CompilationUnit& unit,
  SnapshotProducer producer
) {
  SnapshotModel model;
  model.unit = unit;
  model.producer = std::move(producer);
  model.completeness = std::string(kCompletenessProvenScalarNoop);
  model.root_is_null = true;
  return finish_model(std::move(model), {});
}

SnapshotPublishResult publish_validated_topology(
  const ValidatedArtifact& artifact,
  const styio::config::CompilationUnit& unit,
  SnapshotProducer producer,
  SnapshotFault fault
) {
  SnapshotPublishResult failed;
  if (!artifact.identity_scope().is_globally_comparable()) {
    failed.error = "observable static snapshots require a qualified compilation unit";
    return failed;
  }
  if (!artifact.publication_complete() || fault == SnapshotFault::MissingDescriptor) {
    failed.error = "observable static snapshot publication descriptors are incomplete";
    return failed;
  }

  const auto& nodes = artifact.node_publications();
  const auto& relations = artifact.relation_publications();
  std::vector<std::string> dense_to_public;
  dense_to_public.reserve(nodes.size());

  SnapshotModel model;
  model.unit = unit;
  model.producer = std::move(producer);
  model.completeness = std::string(kCompletenessValidatedTopology);
  model.nodes.reserve(nodes.size());
  model.edges.reserve(relations.size());
  model.facts.reserve(nodes.size() * 2);
  model.evidence.reserve(nodes.size() * 3 + relations.size());

  std::string file_anchor_id;
  bool emitted_file_anchor = false;
  const bool has_source_owned = std::any_of(
    nodes.begin(),
    nodes.end(),
    [](const ValidatedArtifact::NodePublication& node) { return node.source_owned; });
  if (has_source_owned) {
    file_anchor_id = derive_snapshot_id(
      kAnchorIdPrefix,
      "styio.observable-static-snapshot.anchor.v1",
      {unit.package_name, unit.manifest_relative_path, unit.entry_relative_path, unit.entry_relative_path});
    model.anchors.push_back(AnchorRecord{
      file_anchor_id,
      unit.entry_relative_path,
      std::string(kAnchorPrecisionFile)});
    emitted_file_anchor = true;
  }

  std::vector<std::string> node_evidence_ids;
  node_evidence_ids.reserve(nodes.size());
  std::string program_id;

  for (std::size_t index = 0; index < nodes.size(); ++index) {
    const auto& node = nodes[index];
    std::string node_id = std::string(kNodeIdPrefix) + styio::semantic_identity::encode_hex(node.identity);
    if (fault == SnapshotFault::DuplicateIdentity && index == 1 && !dense_to_public.empty()) {
      node_id = dense_to_public.front();
    }
    dense_to_public.push_back(node_id);

    std::vector<std::string> node_anchors;
    if (node.source_owned && emitted_file_anchor) {
      node_anchors.push_back(file_anchor_id);
    }
    std::vector<std::string> evidence_anchors = node_anchors;
    auto evidence = make_evidence(
      unit,
      node_evidence_rule(node.role),
      {node_id},
      {},
      std::move(evidence_anchors));
    node_evidence_ids.push_back(evidence.id);

    NodeRecord record;
    record.id = node_id;
    record.kind = styio::resource_topology::to_string(node.kind);
    record.role = styio::resource_topology::to_string(node.role);
    record.anchors = std::move(node_anchors);
    record.evidence = evidence.id;
    if (node.kind == NodeKind::Program) {
      program_id = node_id;
    }
    model.nodes.push_back(std::move(record));
    model.evidence.push_back(std::move(evidence));

    FactRecord capabilities_fact;
    capabilities_fact.subject = node_id;
    capabilities_fact.predicate = std::string(kFactPredicateCapabilities);
    capabilities_fact.value_is_array = true;
    capabilities_fact.array_value = styio::resource_topology::capability_names(node.capabilities);
    capabilities_fact.id = derive_snapshot_id(
      kFactIdPrefix,
      "styio.observable-static-snapshot.fact.v1",
      {unit.package_name, unit.manifest_relative_path, unit.entry_relative_path, node_id, capabilities_fact.predicate});
    auto cap_evidence = make_evidence(
      unit,
      fact_evidence_rule(capabilities_fact.predicate),
      {capabilities_fact.id},
      {node_evidence_ids.back()},
      {});
    capabilities_fact.evidence = cap_evidence.id;
    model.facts.push_back(std::move(capabilities_fact));
    model.evidence.push_back(std::move(cap_evidence));

    FactRecord state_fact;
    state_fact.subject = node_id;
    state_fact.predicate = std::string(kFactPredicateTypeState);
    state_fact.string_value = styio::resource_topology::to_string(node.type_state);
    state_fact.id = derive_snapshot_id(
      kFactIdPrefix,
      "styio.observable-static-snapshot.fact.v1",
      {unit.package_name, unit.manifest_relative_path, unit.entry_relative_path, node_id, state_fact.predicate});
    auto state_evidence = make_evidence(
      unit,
      fact_evidence_rule(state_fact.predicate),
      {state_fact.id},
      {node_evidence_ids.back()},
      {});
    state_fact.evidence = state_evidence.id;
    model.facts.push_back(std::move(state_fact));
    model.evidence.push_back(std::move(state_evidence));
  }

  if (program_id.empty()) {
    failed.error = "observable static snapshot is missing the Program root";
    return failed;
  }
  model.root = program_id;

  for (const auto& relation : relations) {
    if (fault == SnapshotFault::DanglingEndpoint
        || relation.from >= dense_to_public.size()
        || relation.to >= dense_to_public.size()) {
      failed.error = "observable static snapshot relation endpoint is unresolved";
      return failed;
    }
    EdgeRecord edge;
    edge.kind = styio::resource_topology::to_string(relation.kind);
    edge.from = dense_to_public[relation.from];
    edge.to = dense_to_public[relation.to];
    edge.id = derive_snapshot_id(
      kEdgeIdPrefix,
      "styio.observable-static-snapshot.edge.v1",
      {
        unit.package_name,
        unit.manifest_relative_path,
        unit.entry_relative_path,
        edge.kind,
        edge.from,
        edge.to,
        relation.relation_key,
      });
    auto evidence = make_evidence(
      unit,
      edge_evidence_rule(relation.kind, relation.relation_key),
      {edge.id},
      {node_evidence_ids[relation.from], node_evidence_ids[relation.to]},
      {});
    edge.evidence = evidence.id;
    model.edges.push_back(std::move(edge));
    model.evidence.push_back(std::move(evidence));
  }

  if (fault == SnapshotFault::EvidenceCycle && model.evidence.size() >= 2) {
    model.evidence[0].prerequisites.push_back(model.evidence[1].id);
    model.evidence[1].prerequisites.push_back(model.evidence[0].id);
  }

  return finish_model(std::move(model), {});
}

} // namespace styio::observable
