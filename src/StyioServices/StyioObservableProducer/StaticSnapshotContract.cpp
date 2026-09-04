#include "StaticSnapshotContract.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SHA256.h"

namespace styio::observable {
namespace {

using styio::resource_topology::EdgeKind;
using styio::resource_topology::NodeKind;
using styio::resource_topology::SemanticRole;
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

SnapshotCompilationUnit public_unit(const styio::config::CompilationUnit& unit) {
  return SnapshotCompilationUnit{
    unit.package_name,
    unit.manifest_relative_path,
    unit.entry_relative_path};
}

SnapshotPublishResult to_publish_result(SnapshotIssue issue) {
  SnapshotPublishResult result;
  result.ok = issue.ok;
  result.error = std::move(issue.error);
  result.json = std::move(issue.json);
  result.counts = issue.counts;
  return result;
}

SnapshotEvidence make_evidence(
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

  return SnapshotEvidence{
    std::move(id),
    std::move(producer_rule),
    "1",
    std::move(subjects),
    std::move(prerequisites),
    std::move(anchors)};
}

} // namespace

SnapshotPublishResult publish_proven_scalar_noop(
  const styio::config::CompilationUnit& unit,
  SnapshotProducer producer
) {
  Snapshot snapshot;
  snapshot.compilation_unit = public_unit(unit);
  snapshot.producer = std::move(producer);
  snapshot.completeness = std::string(kCompletenessProvenScalarNoop);
  snapshot.root_is_null = true;
  return to_publish_result(finalize_snapshot(std::move(snapshot)));
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

  Snapshot snapshot;
  snapshot.compilation_unit = public_unit(unit);
  snapshot.producer = std::move(producer);
  snapshot.completeness = std::string(kCompletenessValidatedTopology);
  snapshot.nodes.reserve(nodes.size());
  snapshot.edges.reserve(relations.size());
  snapshot.facts.reserve(nodes.size() * 2);
  snapshot.evidence.reserve(nodes.size() * 3 + relations.size());

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
    snapshot.anchors.push_back(SnapshotAnchor{
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

    SnapshotNode record;
    record.id = node_id;
    record.kind = styio::resource_topology::to_string(node.kind);
    record.role = styio::resource_topology::to_string(node.role);
    record.anchors = std::move(node_anchors);
    record.evidence = evidence.id;
    if (node.kind == NodeKind::Program) {
      program_id = node_id;
    }
    snapshot.nodes.push_back(std::move(record));
    snapshot.evidence.push_back(std::move(evidence));

    SnapshotFact capabilities_fact;
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
    snapshot.facts.push_back(std::move(capabilities_fact));
    snapshot.evidence.push_back(std::move(cap_evidence));

    SnapshotFact state_fact;
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
    snapshot.facts.push_back(std::move(state_fact));
    snapshot.evidence.push_back(std::move(state_evidence));
  }

  if (program_id.empty()) {
    failed.error = "observable static snapshot is missing the Program root";
    return failed;
  }
  snapshot.root = program_id;

  for (const auto& relation : relations) {
    if (fault == SnapshotFault::DanglingEndpoint
        || relation.from >= dense_to_public.size()
        || relation.to >= dense_to_public.size()) {
      failed.error = "observable static snapshot relation endpoint is unresolved";
      return failed;
    }
    SnapshotEdge edge;
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
    snapshot.edges.push_back(std::move(edge));
    snapshot.evidence.push_back(std::move(evidence));
  }

  if (fault == SnapshotFault::EvidenceCycle && snapshot.evidence.size() >= 2) {
    snapshot.evidence[0].prerequisites.push_back(snapshot.evidence[1].id);
    snapshot.evidence[1].prerequisites.push_back(snapshot.evidence[0].id);
  }

  return to_publish_result(finalize_snapshot(std::move(snapshot)));
}

} // namespace styio::observable
