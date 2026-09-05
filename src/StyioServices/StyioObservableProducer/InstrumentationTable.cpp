#include "InstrumentationTable.hpp"

#include "StyioSema/SemaContext.hpp"
#include "StyioServices/StyioObservableProducer/StaticSnapshotContract.hpp"
#include "StyioUtil/SemanticIdentity.hpp"

namespace styio::observable {

InstrumentationTable
build_instrumentation_table(
  const styio::resource_topology::ValidatedArtifact& artifact,
  const Snapshot& snapshot
) {
  InstrumentationTable table;
  table.snapshot_id = snapshot_identity(snapshot);
  table.generation = 1;
  const auto bindings = artifact.source_site_bindings();
  table.descriptors.reserve(bindings.size());
  for (const auto& binding : bindings) {
    SiteRole role = SiteRole::RuntimeOnly;
    if (binding.kind == styio::resource_topology::NodeKind::Task
        || binding.role == styio::resource_topology::SemanticRole::Task) {
      role = SiteRole::Task;
    } else if (binding.kind == styio::resource_topology::NodeKind::Sink
               || binding.role == styio::resource_topology::SemanticRole::Sink) {
      role = SiteRole::Await;
    } else {
      continue;
    }
    InstrumentationDescriptor desc;
    desc.snapshot_id = table.snapshot_id;
    desc.site_id = std::string(kNodeIdPrefix)
      + styio::semantic_identity::encode_hex(binding.identity);
    desc.role = role;
    const auto index = static_cast<std::uint32_t>(table.descriptors.size());
    table.descriptors.push_back(std::move(desc));
    if (binding.source != nullptr) {
      table.ast_index.emplace(binding.source, index);
    }
  }
  return table;
}

bool
bind_runtime_observation(
  StyioSemaContext& sema,
  const MainBlockAST* root,
  const styio::config::CompilationUnit& unit,
  std::string_view producer_version,
  InstrumentationTable& out_table,
  std::string& error_message
) {
  const SnapshotProducer producer{"styio", std::string(producer_version)};
  SnapshotPublishResult published;
  const auto* artifact = sema.resource_topology_artifact_for(root);
  if (artifact == nullptr) {
    if (sema.resource_topology_lifecycle()
        == StyioSemaContext::ResourceTopologyLifecycle::ScalarNoop) {
      published = publish_proven_scalar_noop(unit, producer);
    } else {
      error_message =
        "runtime observation requested but Sema did not publish a topology artifact";
      return false;
    }
  } else {
    published = publish_validated_topology(*artifact, unit, producer);
  }
  if (!published.ok) {
    error_message = published.error;
    return false;
  }
  const SnapshotIssue parsed = parse_snapshot(published.json);
  if (!parsed.ok) {
    error_message = parsed.error;
    return false;
  }
  if (artifact != nullptr) {
    out_table = build_instrumentation_table(*artifact, parsed.snapshot);
  } else {
    out_table = InstrumentationTable{};
    out_table.snapshot_id = snapshot_identity(parsed.snapshot);
  }
  sema.set_runtime_observation_generation(out_table.generation);
  sema.set_runtime_observation_requested(true);
  for (const auto& [ast, index] : out_table.ast_index) {
    StyioSemaContext::RuntimeObservationSiteBinding binding;
    binding.generation = out_table.generation;
    binding.descriptor_index = index;
    binding.role = static_cast<std::uint8_t>(out_table.descriptors[index].role);
    sema.bind_runtime_observation_site(ast, binding);
  }
  return true;
}

} // namespace styio::observable
