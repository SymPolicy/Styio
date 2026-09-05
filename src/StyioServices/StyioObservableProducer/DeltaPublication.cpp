#include "DeltaPublication.hpp"

#include <fstream>
#include <sstream>
#include <system_error>

#include "StyioServices/StyioObservable/Delta.hpp"
#include "StyioServices/StyioObservable/Snapshot.hpp"

namespace styio::observable {

namespace {

bool
read_parent_bytes(const std::filesystem::path& path, std::string& out_text) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (in.bad()) {
    return false;
  }
  out_text = buffer.str();
  return true;
}

bool
same_compilation_unit(
  const SnapshotCompilationUnit& parent,
  const SnapshotCompilationUnit& current
) {
  return parent.package_name == current.package_name
    && parent.manifest_path == current.manifest_path
    && parent.entry_path == current.entry_path;
}

DeltaStageResult
degraded(std::string_view reason) {
  DeltaStageResult result;
  result.reason = std::string(reason);
  return result;
}

} // namespace

DeltaStageResult
publish_compile_plan_delta(
  const std::filesystem::path& parent_snapshot_path,
  std::string_view current_snapshot_json,
  const styio::config::CompilationUnit& unit,
  const std::filesystem::path& artifact_dir,
  std::string_view artifact_stem,
  CompilePlanArtifactWriter write_artifact,
  std::vector<std::filesystem::path>& receipt_artifacts
) {
  std::string parent_text;
  if (!read_parent_bytes(parent_snapshot_path, parent_text)) {
    return degraded(kDeltaReasonParentUnreadable);
  }

  // The parent must decode with the public S2 decoder and must already be
  // canonical bytes: its identity is derived from those bytes, so a
  // non-canonical parent would name a different snapshot than the one on disk.
  const SnapshotIssue parent = parse_snapshot(parent_text);
  if (!parent.ok || parent.json != parent_text) {
    return degraded(kDeltaReasonParentInvalid);
  }

  const SnapshotIssue current = parse_snapshot(current_snapshot_json);
  if (!current.ok || current.json != current_snapshot_json) {
    return degraded(kDeltaReasonDeltaFailed);
  }

  if (parent.snapshot.schema_version != current.snapshot.schema_version
      || parent.snapshot.contract != current.snapshot.contract
      || !same_compilation_unit(parent.snapshot.compilation_unit, current.snapshot.compilation_unit)
      || current.snapshot.compilation_unit.package_name != unit.package_name
      || current.snapshot.compilation_unit.manifest_path != unit.manifest_relative_path
      || current.snapshot.compilation_unit.entry_path != unit.entry_relative_path) {
    return degraded(kDeltaReasonParentMismatch);
  }

  // S2 semantics only: linear merge over canonical categories.  Lineage
  // operations appear when the current snapshot carries producer lineage
  // records; this stage adds no heuristics of its own.
  const DeltaIssue generated = generate_delta(parent.snapshot, current.snapshot);
  if (!generated.ok
      || generated.delta.parent_snapshot_id != parent.snapshot_id
      || generated.delta.target_snapshot_id != current.snapshot_id) {
    return degraded(kDeltaReasonDeltaFailed);
  }

  // Fail closed: the emitted delta must reconstruct the published bytes.
  const DeltaIssue applied = apply_delta(parent.snapshot, generated.delta);
  if (!applied.ok || serialize_snapshot(applied.snapshot) != current_snapshot_json) {
    return degraded(kDeltaReasonDeltaFailed);
  }

  DeltaStageResult result;
  result.parent_snapshot_id = generated.delta.parent_snapshot_id;
  result.target_snapshot_id = generated.delta.target_snapshot_id;
  result.artifact_path =
    artifact_dir / (std::string(artifact_stem) + std::string(kDeltaArtifactSuffix));
  std::string artifact_error;
  if (!write_artifact(result.artifact_path, generated.json, artifact_error)) {
    std::error_code remove_error;
    std::filesystem::remove(result.artifact_path, remove_error);
    return degraded(kDeltaReasonWriteFailed);
  }
  receipt_artifacts.push_back(result.artifact_path);
  result.published = true;
  result.operation_count = generated.delta.operations.size();
  result.serialized_bytes = generated.json.size();
  return result;
}

std::string
delta_receipt_json(const DeltaStageResult& result) {
  std::string json = "{\"delta\":\"";
  if (result.published) {
    json.append(kDeltaReceiptPublished.data(), kDeltaReceiptPublished.size());
    json += "\",\"parent_snapshot_id\":\"" + result.parent_snapshot_id
      + "\",\"target_snapshot_id\":\"" + result.target_snapshot_id + "\"}";
    return json;
  }
  json.append(kDeltaReceiptFullSnapshotRequired.data(), kDeltaReceiptFullSnapshotRequired.size());
  json += "\",\"reason\":\"" + result.reason + "\"}";
  return json;
}

} // namespace styio::observable
