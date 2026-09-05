#ifndef STYIO_CONFIG_COMPILE_PLAN_CONTRACT_HPP_
#define STYIO_CONFIG_COMPILE_PLAN_CONTRACT_HPP_

#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "StyioServices/StyioObservable/RuntimeCorrelation.hpp"

namespace styio::config {

struct CompilationUnit
{
  std::string package_name;
  std::string manifest_relative_path;
  std::string entry_relative_path;
};

struct CompilePlanRequest
{
  std::filesystem::path plan_path;
  int plan_version = 0;
  std::string intent;
  std::filesystem::path workspace_root;
  std::string generated_by_tool;
  std::string entry_package_id;
  std::string entry_target_kind;
  std::string entry_target_name;
  std::filesystem::path entry_file;
  std::filesystem::path build_root;
  std::filesystem::path artifact_dir;
  std::filesystem::path diag_dir;
  std::string error_format = "text";
  bool emit_ast = false;
  bool emit_styio_ir = false;
  bool emit_llvm_ir = false;
  bool emit_observable_static_snapshot = false;
  int observable_static_snapshot_schema_version = 0;
  std::vector<std::string> observable_static_snapshot_required_capabilities;
  // Optional `emit.observable_static_snapshot.parent_snapshot_path`: the
  // previous snapshot artifact the producer should diff against.  Transport
  // input only; it never participates in admission, identity, or snapshot
  // bytes.  Empty when the request names no parent.
  std::filesystem::path observable_static_snapshot_parent_snapshot_path;
  bool emit_runtime_observation = false;
  int runtime_observation_schema_version = 0;
  std::vector<std::string> runtime_observation_required_capabilities;
  styio::observable::ObservationMode runtime_observation_mode =
    styio::observable::ObservationMode::Aggregate;
  std::uint32_t runtime_observation_lane_capacity =
    styio::observable::kDefaultLaneCapacity;
  std::uint32_t runtime_observation_priority_reserved =
    styio::observable::kDefaultPriorityReserved;
  std::uint32_t runtime_observation_producer_lanes = 0;
  styio::observable::SamplingSpec runtime_observation_sampling;
  std::optional<CompilationUnit> compilation_unit;
};

bool
probe_compile_plan_diag_dir(
  const std::filesystem::path& plan_path,
  std::filesystem::path& out_diag_dir
);

bool
parse_compile_plan(
  const std::filesystem::path& plan_path,
  CompilePlanRequest& out_request,
  std::string& error_message
);

bool
parse_compile_plan(
  const std::filesystem::path& plan_path,
  CompilePlanRequest& out_request,
  std::string& error_message,
  std::string& error_subcode
);

// Output stem shared by every compile-plan artifact under `artifact_dir`
// (`<stem>.typed.ast.txt`, `<stem>.observable-static-snapshot.json`, ...).
// Prefers the entry target name, falls back to the entry file stem, then to
// `entry`, and replaces every character outside `[A-Za-z0-9-_.]` with `_`.
// Header-inline so the nano driver, which does not link the contract library,
// can still compile its unreachable compile-plan paths.
inline std::string
compile_plan_artifact_stem(const CompilePlanRequest& request) {
  std::string stem = request.entry_target_name.empty()
                       ? request.entry_file.stem().string()
                       : request.entry_target_name;
  if (stem.empty()) {
    stem = "entry";
  }
  for (char& ch : stem) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!(std::isalnum(uch) || ch == '-' || ch == '_' || ch == '.')) {
      ch = '_';
    }
  }
  return stem;
}

} // namespace styio::config

#endif
