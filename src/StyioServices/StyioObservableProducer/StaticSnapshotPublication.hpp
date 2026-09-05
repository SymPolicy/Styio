#pragma once
#ifndef STYIO_OBSERVABLE_STATIC_SNAPSHOT_PUBLICATION_HPP_
#define STYIO_OBSERVABLE_STATIC_SNAPSHOT_PUBLICATION_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "StyioServices/StyioConfig/CompilePlanContract.hpp"
#include "StyioServices/StyioConfig/NanoProfile.hpp"
#include "StyioServices/StyioObservableProducer/StaticSnapshotContract.hpp"
#include "StyioUtil/SemanticIdentity.hpp"

class MainBlockAST;
class StyioSemaContext;

namespace styio::profiler {
class FrontendProfiler;
}

namespace styio::observable {

// CLI stage wiring for the incubating schema-v1 static snapshot.  The full
// `styio` driver calls these entry points; nano advertises empty arrays and
// links no snapshot publication code.

// Diagnostic subcode attributed to every snapshot stage failure.
inline constexpr std::string_view kStaticSnapshotDiagnosticSubcode =
  "observable_static_snapshot";

// Profiler phase recorded around the publication stage.
inline constexpr const char* kStaticSnapshotProfilePhase =
  "observable_static_snapshot";

// `--machine-info=json` value for the `observable_static_snapshot` key:
// `{"schema_versions":[...],"capabilities":[...],"optional_capabilities":[...]}`.
// `capabilities` is the closed set admissible as `required_capabilities`;
// `optional_capabilities` (`producer-lineage`, `snapshot-delta`) are
// independently negotiated and never admissible as required.
std::string advertised_static_snapshot_machine_info_json();

// `--machine-info=json` value for the `observable_delta` key:
// `{"schema_versions":[{"major":0,"minor":1}]}`.
std::string advertised_delta_machine_info_json();

inline std::string
static_snapshot_machine_info_json() {
#if STYIO_NANO_BUILD
  return "{\"schema_versions\":[],\"capabilities\":[],\"optional_capabilities\":[]}";
#else
  return advertised_static_snapshot_machine_info_json();
#endif
}

inline std::string
delta_machine_info_json() {
#if STYIO_NANO_BUILD
  return "{\"schema_versions\":[]}";
#else
  return advertised_delta_machine_info_json();
#endif
}

// Semantic identity scope for the Sema context of one compile-plan run.  A
// requested snapshot with an admitted compilation unit gets the qualified
// PLAN-003 scope; every other run (including nano) stays anonymous.
inline styio::semantic_identity::Scope
static_snapshot_identity_scope(
  [[maybe_unused]] const std::optional<styio::config::CompilePlanRequest>& request
) {
#if !STYIO_NANO_BUILD
  if (request.has_value()
      && request->emit_observable_static_snapshot
      && request->compilation_unit.has_value()) {
    return styio::semantic_identity::Scope::qualified(
      request->compilation_unit->package_name,
      request->compilation_unit->manifest_relative_path,
      request->compilation_unit->entry_relative_path);
  }
#endif
  return styio::semantic_identity::Scope::anonymous();
}

// Writes one compile-plan artifact (creating its parent directory) and
// reports a human-readable error on failure.  The driver supplies its own
// writer so snapshot bytes share the artifact path of every other emission.
using CompilePlanArtifactWriter = bool (*)(
  const std::filesystem::path& path,
  const std::string& text,
  std::string& error_message
);

struct StaticSnapshotStageResult
{
  bool ok = false;
  // Non-empty when `ok` is false; attribute with kStaticSnapshotDiagnosticSubcode.
  std::string error;
  std::filesystem::path artifact_path;
  // Compact JSON object for the `observable_static_snapshot` receipt key.
  // Empty when the request names no `parent_snapshot_path`; otherwise the
  // delta publication record (see DeltaPublication.hpp).
  std::string receipt_json;
};

// Runs the admitted publication stage after Sema and before lowering:
// selects proven-scalar-noop or validated-topology publication from the Sema
// lifecycle, writes `<artifact_dir>/<artifact_stem>.observable-static-snapshot.json`
// (removing any partial file on write failure), appends the path to the
// receipt artifact list, and records the snapshot profiler phase and counters.
// When the request names a `parent_snapshot_path`, the delta stage runs after
// the snapshot is written; its outcome never fails the snapshot publication.
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
);

} // namespace styio::observable

#endif
