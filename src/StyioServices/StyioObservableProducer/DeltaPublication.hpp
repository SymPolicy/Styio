#pragma once
#ifndef STYIO_OBSERVABLE_DELTA_PUBLICATION_HPP_
#define STYIO_OBSERVABLE_DELTA_PUBLICATION_HPP_

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "StyioServices/StyioConfig/CompilePlanContract.hpp"
#include "StyioServices/StyioObservableProducer/StaticSnapshotPublication.hpp"

namespace styio::observable {

// Producer-owned delta emission between the previous snapshot artifact named
// by `emit.observable_static_snapshot.parent_snapshot_path` and the snapshot
// just published.  Runs only after a successful snapshot publication and only
// when a parent path is present; every failure keeps the snapshot publication
// successful and degrades the receipt to `full_snapshot_required`.

inline constexpr std::string_view kDeltaArtifactSuffix = ".observable-delta.json";

// `receipt.json` key carrying the delta publication record.
inline constexpr std::string_view kStaticSnapshotReceiptKey =
  "observable_static_snapshot";

// Receipt `delta` values.
inline constexpr std::string_view kDeltaReceiptPublished = "published";
inline constexpr std::string_view kDeltaReceiptFullSnapshotRequired =
  "full_snapshot_required";

// Closed `reason` subcode set recorded with `full_snapshot_required`.
inline constexpr std::string_view kDeltaReasonParentUnreadable = "parent_unreadable";
inline constexpr std::string_view kDeltaReasonParentInvalid = "parent_invalid";
inline constexpr std::string_view kDeltaReasonParentMismatch = "parent_mismatch";
inline constexpr std::string_view kDeltaReasonDeltaFailed = "delta_failed";
inline constexpr std::string_view kDeltaReasonWriteFailed = "write_failed";

struct DeltaStageResult
{
  bool published = false;
  // One of the closed kDeltaReason* subcodes when `published` is false.
  std::string reason;
  std::string parent_snapshot_id;
  std::string target_snapshot_id;
  std::filesystem::path artifact_path;
  std::size_t operation_count = 0;
  std::size_t serialized_bytes = 0;
};

// Reads and decodes the parent artifact with the public S2 decoder, requires
// the same schema version and compilation unit as the current snapshot,
// generates and validates the delta with the S2 library, writes
// `<artifact_dir>/<artifact_stem>.observable-delta.json` (removing any partial
// file on write failure), and appends the path to the receipt artifact list.
DeltaStageResult
publish_compile_plan_delta(
  const std::filesystem::path& parent_snapshot_path,
  std::string_view current_snapshot_json,
  const styio::config::CompilationUnit& unit,
  const std::filesystem::path& artifact_dir,
  std::string_view artifact_stem,
  CompilePlanArtifactWriter write_artifact,
  std::vector<std::filesystem::path>& receipt_artifacts
);

// Compact JSON object recorded under the `observable_static_snapshot` receipt
// key: `{"delta":"published","parent_snapshot_id":"s1_…","target_snapshot_id":"s1_…"}`
// or `{"delta":"full_snapshot_required","reason":"<subcode>"}`.
std::string delta_receipt_json(const DeltaStageResult& result);

} // namespace styio::observable

#endif
