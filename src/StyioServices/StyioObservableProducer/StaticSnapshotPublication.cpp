#include "StaticSnapshotPublication.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <system_error>

#include "StyioProfiler/FrontendProfiler.hpp"
#include "StyioSema/SemaContext.hpp"
#include "StyioServices/StyioObservable/Delta.hpp"
#include "StyioServices/StyioObservableProducer/DeltaPublication.hpp"

namespace styio::observable {

namespace {

template <typename Range>
void
append_string_array(std::string& json, const Range& values) {
  json += "[";
  bool first = true;
  for (const std::string_view value : values) {
    if (!first) {
      json += ",";
    }
    first = false;
    json += "\"";
    json.append(value.data(), value.size());
    json += "\"";
  }
  json += "]";
}

} // namespace

std::string
advertised_static_snapshot_machine_info_json() {
  std::string json = "{\"schema_versions\":[1],\"capabilities\":";
  append_string_array(json, kStaticSnapshotCapabilities);
  json += ",\"optional_capabilities\":";
  constexpr std::array<std::string_view, 2> optional{kLineageCapability, kDeltaCapability};
  append_string_array(json, optional);
  json += "}";
  return json;
}

std::string
advertised_delta_machine_info_json() {
  return "{\"schema_versions\":[{\"major\":" + std::to_string(kDeltaSchemaMajor)
    + ",\"minor\":" + std::to_string(kDeltaSchemaMinor) + "}]}";
}

StaticSnapshotStageResult
publish_compile_plan_static_snapshot(
  const styio::config::CompilePlanRequest& request,
  std::string_view artifact_stem,
  const StyioSemaContext& sema,
  const MainBlockAST* root,
  std::string_view producer_version,
  CompilePlanArtifactWriter write_artifact,
  std::vector<std::filesystem::path>& receipt_artifacts,
  styio::profiler::FrontendProfiler& profiler
) {
  StaticSnapshotStageResult result;
  auto profile_phase = profiler.phase(kStaticSnapshotProfilePhase);
  if (!request.compilation_unit.has_value()) {
    result.error =
      "observable static snapshot request is missing a qualified compilation unit";
    return result;
  }
  const SnapshotProducer producer{"styio", std::string(producer_version)};
  SnapshotPublishResult published;
  if (sema.resource_topology_lifecycle()
      == StyioSemaContext::ResourceTopologyLifecycle::ScalarNoop) {
    published = publish_proven_scalar_noop(*request.compilation_unit, producer);
  } else {
    const auto* artifact = sema.resource_topology_artifact_for(root);
    if (artifact == nullptr) {
      result.error =
        "observable static snapshot requested but Sema did not publish a topology artifact";
      return result;
    }
    published = publish_validated_topology(*artifact, *request.compilation_unit, producer);
  }
  if (!published.ok) {
    result.error = published.error;
    return result;
  }
  result.artifact_path =
    request.artifact_dir
    / (std::string(artifact_stem) + std::string(kStaticSnapshotArtifactSuffix));
  std::string artifact_error;
  if (!write_artifact(result.artifact_path, published.json, artifact_error)) {
    std::error_code remove_error;
    std::filesystem::remove(result.artifact_path, remove_error);
    result.error = artifact_error;
    return result;
  }
  receipt_artifacts.push_back(result.artifact_path);
  profiler.add_counter(
    "snapshot_node_count",
    static_cast<std::int64_t>(published.counts.nodes));
  profiler.add_counter(
    "snapshot_edge_count",
    static_cast<std::int64_t>(published.counts.edges));
  profiler.add_counter(
    "snapshot_fact_count",
    static_cast<std::int64_t>(published.counts.facts));
  profiler.add_counter(
    "snapshot_anchor_count",
    static_cast<std::int64_t>(published.counts.anchors));
  profiler.add_counter(
    "snapshot_evidence_count",
    static_cast<std::int64_t>(published.counts.evidence));
  profiler.add_counter(
    "snapshot_serialized_bytes",
    static_cast<std::int64_t>(published.counts.serialized_bytes));
  result.ok = true;

  if (!request.observable_static_snapshot_parent_snapshot_path.empty()) {
    const DeltaStageResult delta = publish_compile_plan_delta(
      request.observable_static_snapshot_parent_snapshot_path,
      published.json,
      *request.compilation_unit,
      request.artifact_dir,
      artifact_stem,
      write_artifact,
      receipt_artifacts);
    result.receipt_json = delta_receipt_json(delta);
    profiler.add_counter(
      "delta_operation_count",
      static_cast<std::int64_t>(delta.operation_count));
    profiler.add_counter(
      "delta_serialized_bytes",
      static_cast<std::int64_t>(delta.serialized_bytes));
  }
  return result;
}

} // namespace styio::observable
