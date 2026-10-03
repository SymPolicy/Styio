#include "CompilePlanContract.hpp"

#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_set>

#include "StyioUtil/SemanticIdentity.hpp"
#include "llvm/Support/Error.h"
#include "llvm/Support/JSON.h"

namespace styio::config {

namespace {

bool
read_text_file(
  const std::filesystem::path& path,
  std::string& out_text,
  std::string& error_message
) {
  std::ifstream in(path);
  if (!in.is_open()) {
    error_message = "cannot read file: " + path.string();
    return false;
  }

  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (in.bad()) {
    error_message = "cannot read file: " + path.string();
    return false;
  }

  out_text = buffer.str();
  return true;
}

bool
json_require_string(
  const llvm::json::Object& obj,
  const char* key,
  std::string& out_value,
  std::string& error_message,
  const char* field_path = nullptr
) {
  const char* effective_field = field_path == nullptr ? key : field_path;
  const llvm::json::Value* value = obj.get(key);
  if (value == nullptr) {
    error_message = std::string("compile-plan is missing required string field: ") + key;
    return false;
  }

  const auto raw = value->getAsString();
  if (!raw.has_value()) {
    error_message = std::string("compile-plan field must be a non-empty string: ") + effective_field;
    return false;
  }
  if (raw->empty()) {
    error_message =
      std::string("compile-plan string field must be a non-empty string: ") + effective_field;
    return false;
  }
  out_value = std::string(*raw);
  return true;
}

bool
json_require_integer(
  const llvm::json::Object& obj,
  const char* key,
  std::int64_t& out_value,
  std::string& error_message
) {
  const auto raw = obj.getInteger(key);
  if (!raw.has_value()) {
    error_message = std::string("compile-plan is missing required integer field: ") + key;
    return false;
  }
  out_value = *raw;
  return true;
}

bool
json_require_bool(
  const llvm::json::Object& obj,
  const char* key,
  bool& out_value,
  std::string& error_message
) {
  const auto raw = obj.getBoolean(key);
  if (!raw.has_value()) {
    error_message = std::string("compile-plan is missing required boolean field: ") + key;
    return false;
  }
  out_value = *raw;
  return true;
}

bool
json_require_object(
  const llvm::json::Object& obj,
  const char* key,
  const llvm::json::Object*& out_value,
  std::string& error_message
) {
  out_value = obj.getObject(key);
  if (out_value == nullptr) {
    error_message = std::string("compile-plan is missing required object field: ") + key;
    return false;
  }
  return true;
}

bool
json_require_array(
  const llvm::json::Object& obj,
  const char* key,
  const llvm::json::Array*& out_value,
  std::string& error_message
) {
  out_value = obj.getArray(key);
  if (out_value == nullptr) {
    error_message = std::string("compile-plan is missing required array field: ") + key;
    return false;
  }
  return true;
}

bool
compile_plan_validate_packages(
  const llvm::json::Array& packages,
  const std::string& entry_package_id,
  std::string& error_message
) {
  bool saw_entry_package = false;
  for (size_t i = 0; i < packages.size(); ++i) {
    const llvm::json::Object* package = packages[i].getAsObject();
    if (package == nullptr) {
      error_message =
        "compile-plan packages[" + std::to_string(i) + "] must be an object";
      return false;
    }

    const std::string package_key = "packages[" + std::to_string(i) + "].id";
    std::string package_id;
    if (!json_require_string(*package, "id", package_id, error_message, package_key.c_str())) {
      return false;
    }
    if (package_id == entry_package_id) {
      saw_entry_package = true;
    }
  }

  if (!saw_entry_package) {
    error_message =
      "compile-plan entry.package_id is not present in packages: " + entry_package_id;
    return false;
  }
  return true;
}

bool
compile_plan_require_absolute_path(
  const llvm::json::Object& obj,
  const char* key,
  std::filesystem::path& out_value,
  std::string& error_message,
  const char* field_path = nullptr
) {
  std::string raw_value;
  if (!json_require_string(obj, key, raw_value, error_message, field_path)) {
    return false;
  }

  out_value = std::filesystem::path(raw_value);
  if (!out_value.is_absolute()) {
    const char* effective_field = field_path == nullptr ? key : field_path;
    error_message = std::string("compile-plan path must be absolute: ") + effective_field;
    return false;
  }
  return true;
}

constexpr std::array<std::string_view, 5> kStaticSnapshotCapabilities{
  "file-source-anchors",
  "producer-evidence",
  "static-topology-edges",
  "static-topology-facts",
  "static-topology-nodes",
};

bool
snapshot_capability_is_supported(std::string_view name) {
  for (const std::string_view candidate : kStaticSnapshotCapabilities) {
    if (candidate == name) {
      return true;
    }
  }
  return false;
}

bool
package_name_is_valid(std::string_view name) {
  if (name.empty() || name == "." || name == "..") {
    return false;
  }
  return name.find('/') == std::string_view::npos
    && name.find('\\') == std::string_view::npos
    && name.find('@') == std::string_view::npos
    && name.find(' ') == std::string_view::npos;
}

bool
slash_relative_from_root(
  const std::filesystem::path& root,
  const std::filesystem::path& file,
  std::string& out_relative,
  std::string& error_message,
  const char* field_name
) {
  if (!root.is_absolute() || !file.is_absolute()) {
    error_message = std::string("compile-plan path must be absolute: ") + field_name;
    return false;
  }
  const std::filesystem::path normal_root = root.lexically_normal();
  const std::filesystem::path normal_file = file.lexically_normal();
  const std::filesystem::path relative = normal_file.lexically_relative(normal_root);
  if (relative.empty() || relative.is_absolute()) {
    error_message =
      std::string("compile-plan ") + field_name + " must be contained by the matched package root";
    return false;
  }
  const std::string generic = relative.generic_string();
  if (generic == ".." || generic.rfind("../", 0) == 0 || generic.find("/../") != std::string::npos) {
    error_message =
      std::string("compile-plan ") + field_name + " must be contained by the matched package root";
    return false;
  }
  if (styio::semantic_identity::canonical_relative_path_error(generic)
      != styio::semantic_identity::CanonicalRelativePathError::None) {
    error_message =
      std::string("compile-plan ") + field_name
      + " must normalize to a canonical package-relative slash path";
    return false;
  }
  out_relative = generic;
  return true;
}

bool
parse_observable_static_snapshot_request(
  const llvm::json::Object& emit,
  CompilePlanRequest& out_request,
  std::string& error_message,
  std::string& error_subcode
) {
  const llvm::json::Value* snapshot_value = emit.get("observable_static_snapshot");
  if (snapshot_value == nullptr) {
    out_request.emit_observable_static_snapshot = false;
    return true;
  }

  const llvm::json::Object* snapshot = snapshot_value->getAsObject();
  if (snapshot == nullptr) {
    error_subcode = "observable_static_snapshot_malformed";
    error_message =
      "compile-plan emit.observable_static_snapshot must be an object with schema_version and required_capabilities";
    return false;
  }

  for (const auto& field : *snapshot) {
    const std::string field_name = field.first.str();
    if (field_name != "schema_version"
        && field_name != "required_capabilities"
        && field_name != "parent_snapshot_path") {
      error_subcode = "observable_static_snapshot_malformed";
      error_message =
        "compile-plan emit.observable_static_snapshot contains an unsupported field: "
        + field_name;
      return false;
    }
  }

  std::int64_t schema_version = 0;
  if (!json_require_integer(*snapshot, "schema_version", schema_version, error_message)) {
    error_subcode = "observable_static_snapshot_malformed";
    error_message =
      "compile-plan emit.observable_static_snapshot.schema_version must be an integer";
    return false;
  }
  if (schema_version != 1) {
    error_subcode = "observable_static_snapshot_unsupported_version";
    error_message =
      "unsupported observable static snapshot schema_version: " + std::to_string(schema_version);
    return false;
  }

  const llvm::json::Array* required = snapshot->getArray("required_capabilities");
  if (required == nullptr) {
    error_subcode = "observable_static_snapshot_malformed";
    error_message =
      "compile-plan emit.observable_static_snapshot.required_capabilities must be an array of strings";
    return false;
  }

  std::vector<std::string> required_capabilities;
  required_capabilities.reserve(required->size());
  std::unordered_set<std::string> seen;
  for (size_t i = 0; i < required->size(); ++i) {
    const auto raw = (*required)[i].getAsString();
    if (!raw.has_value() || raw->empty()) {
      error_subcode = "observable_static_snapshot_malformed";
      error_message =
        "compile-plan emit.observable_static_snapshot.required_capabilities["
        + std::to_string(i) + "] must be a non-empty string";
      return false;
    }
    const std::string capability = std::string(*raw);
    if (!snapshot_capability_is_supported(capability)) {
      error_subcode = "observable_static_snapshot_unsupported_capability";
      error_message = "unsupported required observable static snapshot capability: " + capability;
      return false;
    }
    if (!seen.insert(capability).second) {
      error_subcode = "observable_static_snapshot_malformed";
      error_message =
        "compile-plan emit.observable_static_snapshot.required_capabilities contains a duplicate: "
        + capability;
      return false;
    }
    required_capabilities.push_back(capability);
  }

  std::filesystem::path parent_snapshot_path;
  if (const llvm::json::Value* parent_value = snapshot->get("parent_snapshot_path");
      parent_value != nullptr) {
    const auto raw = parent_value->getAsString();
    if (!raw.has_value() || raw->empty()) {
      error_subcode = "observable_static_snapshot_malformed";
      error_message =
        "compile-plan emit.observable_static_snapshot.parent_snapshot_path must be a non-empty string";
      return false;
    }
    parent_snapshot_path = std::filesystem::path(std::string(*raw));
  }

  out_request.emit_observable_static_snapshot = true;
  out_request.observable_static_snapshot_schema_version = static_cast<int>(schema_version);
  out_request.observable_static_snapshot_required_capabilities = std::move(required_capabilities);
  out_request.observable_static_snapshot_parent_snapshot_path = std::move(parent_snapshot_path);
  return true;
}

bool
parse_runtime_observation_request(
  const llvm::json::Object& emit,
  CompilePlanRequest& out_request,
  std::string& error_message,
  std::string& error_subcode
) {
  const llvm::json::Value* observation_value = emit.get("runtime_observation");
  if (observation_value == nullptr) {
    out_request.emit_runtime_observation = false;
    return true;
  }

  const llvm::json::Object* observation = observation_value->getAsObject();
  if (observation == nullptr) {
    error_subcode = "runtime_observation_malformed";
    error_message =
      "compile-plan emit.runtime_observation must be an object with version";
    return false;
  }

  for (const auto& field : *observation) {
    const std::string field_name = field.first.str();
    if (field_name != "version"
        && field_name != "mode"
        && field_name != "required_capabilities"
        && field_name != "lane_capacity"
        && field_name != "priority_reserved"
        && field_name != "producer_lanes"
        && field_name != "sampling") {
      error_subcode = "runtime_observation_malformed";
      error_message =
        "compile-plan emit.runtime_observation contains an unsupported field: "
        + field_name;
      return false;
    }
  }

  std::int64_t version = 0;
  if (!json_require_integer(*observation, "version", version, error_message)) {
    error_subcode = "runtime_observation_malformed";
    error_message = "compile-plan emit.runtime_observation.version must be an integer";
    return false;
  }
  if (version == 1) {
    error_subcode = "runtime_events_unsupported_version";
    error_message = "unsupported runtime-events schema_version: 1";
    return false;
  }
  if (version != styio::observable::kRuntimeEventsSchemaVersion) {
    error_subcode = "runtime_events_unknown_version";
    error_message =
      "unknown runtime-events schema_version: " + std::to_string(version);
    return false;
  }

  styio::observable::ObservationMode mode =
    styio::observable::ObservationMode::Aggregate;
  if (const llvm::json::Value* mode_value = observation->get("mode"); mode_value != nullptr) {
    const auto raw = mode_value->getAsString();
    if (!raw.has_value()
        || !styio::observable::observation_mode_from_text(std::string(*raw), mode)
        || mode == styio::observable::ObservationMode::Disabled) {
      error_subcode = "runtime_observation_unsupported_mode";
      error_message = "unsupported runtime observation mode";
      return false;
    }
  }

  std::vector<std::string> required_capabilities;
  if (const llvm::json::Array* required = observation->getArray("required_capabilities");
      required != nullptr) {
    std::unordered_set<std::string> seen;
    for (size_t i = 0; i < required->size(); ++i) {
      const auto raw = (*required)[i].getAsString();
      if (!raw.has_value() || raw->empty()) {
        error_subcode = "runtime_observation_malformed";
        error_message =
          "compile-plan emit.runtime_observation.required_capabilities["
          + std::to_string(i) + "] must be a non-empty string";
        return false;
      }
      const std::string capability = std::string(*raw);
      if (styio::observable::capability_is_unavailable(capability)
          || !styio::observable::capability_is_supported(capability)) {
        error_subcode = "runtime_observation_unsupported_capability";
        error_message = "unsupported required runtime observation capability: " + capability;
        return false;
      }
      if (!seen.insert(capability).second) {
        error_subcode = "runtime_observation_malformed";
        error_message =
          "compile-plan emit.runtime_observation.required_capabilities contains a duplicate: "
          + capability;
        return false;
      }
      required_capabilities.push_back(capability);
    }
  }

  std::uint32_t lane_capacity = styio::observable::kDefaultLaneCapacity;
  if (const auto raw = observation->getInteger("lane_capacity"); raw.has_value()) {
    if (*raw < 0 || *raw > 0xffffffffll
        || !styio::observable::lane_capacity_is_valid(static_cast<std::uint32_t>(*raw))) {
      error_subcode = "runtime_observation_invalid_bounds";
      error_message = "invalid runtime observation lane_capacity";
      return false;
    }
    lane_capacity = static_cast<std::uint32_t>(*raw);
  }

  std::uint32_t priority_reserved = styio::observable::kDefaultPriorityReserved;
  if (const auto raw = observation->getInteger("priority_reserved"); raw.has_value()) {
    if (*raw < 0 || static_cast<std::uint32_t>(*raw) >= lane_capacity) {
      error_subcode = "runtime_observation_invalid_bounds";
      error_message = "invalid runtime observation priority_reserved";
      return false;
    }
    priority_reserved = static_cast<std::uint32_t>(*raw);
  }

  std::uint32_t producer_lanes = 0;
  if (const auto raw = observation->getInteger("producer_lanes"); raw.has_value()) {
    if (*raw < 0 || *raw > 64) {
      error_subcode = "runtime_observation_invalid_bounds";
      error_message = "invalid runtime observation producer_lanes";
      return false;
    }
    producer_lanes = static_cast<std::uint32_t>(*raw);
  }

  styio::observable::SamplingSpec sampling;
  if (const llvm::json::Value* sampling_value = observation->get("sampling");
      sampling_value != nullptr) {
    const llvm::json::Object* sampling_object = sampling_value->getAsObject();
    if (sampling_object == nullptr) {
      error_subcode = "runtime_observation_malformed";
      error_message = "compile-plan emit.runtime_observation.sampling must be an object";
      return false;
    }
    if (const auto numerator = sampling_object->getInteger("numerator"); numerator.has_value()) {
      if (*numerator <= 0) {
        error_subcode = "runtime_observation_invalid_bounds";
        error_message = "invalid runtime observation sampling.numerator";
        return false;
      }
      sampling.numerator = static_cast<std::uint32_t>(*numerator);
    }
    if (const auto denominator = sampling_object->getInteger("denominator");
        denominator.has_value()) {
      if (*denominator <= 0) {
        error_subcode = "runtime_observation_invalid_bounds";
        error_message = "invalid runtime observation sampling.denominator";
        return false;
      }
      sampling.denominator = static_cast<std::uint32_t>(*denominator);
    }
    if (const auto seed = sampling_object->getInteger("seed"); seed.has_value()) {
      if (*seed < 0) {
        error_subcode = "runtime_observation_invalid_bounds";
        error_message = "invalid runtime observation sampling.seed";
        return false;
      }
      sampling.seed = static_cast<std::uint64_t>(*seed);
    }
    if (sampling.numerator > sampling.denominator) {
      error_subcode = "runtime_observation_invalid_bounds";
      error_message = "invalid runtime observation sampling ratio";
      return false;
    }
  }

  out_request.emit_runtime_observation = true;
  out_request.runtime_observation_schema_version = static_cast<int>(version);
  out_request.runtime_observation_required_capabilities = std::move(required_capabilities);
  out_request.runtime_observation_mode = mode;
  out_request.runtime_observation_lane_capacity = lane_capacity;
  out_request.runtime_observation_priority_reserved = priority_reserved;
  out_request.runtime_observation_producer_lanes = producer_lanes;
  out_request.runtime_observation_sampling = sampling;
  return true;
}

bool
admit_qualified_compilation_unit(
  const llvm::json::Array& packages,
  const std::string& generated_by_tool,
  const std::string& entry_package_id,
  const std::filesystem::path& entry_file,
  CompilePlanRequest& out_request,
  std::string& error_message,
  std::string& error_subcode
) {
  if (generated_by_tool == "styio") {
    error_subcode = out_request.emit_runtime_observation && !out_request.emit_observable_static_snapshot
      ? "runtime_observation_malformed"
      : "observable_static_snapshot_direct_file";
    error_message =
      "direct-file and Styio-produced compile plans cannot publish observable static snapshots";
    return false;
  }
  if (generated_by_tool != "pafio") {
    error_subcode = "observable_static_snapshot_styio_produced";
    error_message =
      "observable static snapshots require a Pafio-produced compile plan";
    return false;
  }

  std::size_t match_count = 0;
  const llvm::json::Object* matched = nullptr;
  for (size_t i = 0; i < packages.size(); ++i) {
    const llvm::json::Object* package = packages[i].getAsObject();
    if (package == nullptr) {
      continue;
    }
    const auto package_id = package->getString("id");
    if (package_id.has_value() && *package_id == entry_package_id) {
      ++match_count;
      matched = package;
    }
  }
  if (match_count == 0) {
    error_subcode = "observable_static_snapshot_unmatched";
    error_message =
      "compile-plan entry.package_id is not present in packages: " + entry_package_id;
    return false;
  }
  if (match_count > 1) {
    error_subcode = match_count == 2
      ? "observable_static_snapshot_duplicate"
      : "observable_static_snapshot_ambiguous";
    error_message =
      "compile-plan entry.package_id matches more than one package record: " + entry_package_id;
    return false;
  }

  std::string package_name;
  std::filesystem::path package_root;
  std::filesystem::path manifest_path;
  if (!json_require_string(*matched, "name", package_name, error_message, "packages[].name")
      || !compile_plan_require_absolute_path(
        *matched, "root", package_root, error_message, "packages[].root")
      || !compile_plan_require_absolute_path(
        *matched, "manifest", manifest_path, error_message, "packages[].manifest")) {
    error_subcode = "observable_static_snapshot_anonymous";
    if (error_message.find("packages[]") == std::string::npos) {
      error_message =
        "observable static snapshots require one qualified package name, root, and manifest";
    }
    return false;
  }
  if (!package_name_is_valid(package_name)) {
    error_subcode = "observable_static_snapshot_anonymous";
    error_message =
      "compile-plan package name must be a namespaced identity and must not be path-shaped";
    return false;
  }

  std::string manifest_relative;
  std::string entry_relative;
  if (!slash_relative_from_root(
        package_root, manifest_path, manifest_relative, error_message, "packages[].manifest")
      || !slash_relative_from_root(
        package_root, entry_file, entry_relative, error_message, "entry.file")) {
    error_subcode = "observable_static_snapshot_escaping_path";
    return false;
  }

  out_request.compilation_unit = CompilationUnit{
    std::move(package_name),
    std::move(manifest_relative),
    std::move(entry_relative)};
  return true;
}

bool
compile_plan_profile_has_only_v1_fields(
  const llvm::json::Object& profile,
  std::string& error_message
) {
  constexpr std::array<std::string_view, 4> allowed{
    "name",
    "opt_level",
    "debug",
    "lto",
  };
  for (const auto& field : profile) {
    const std::string field_name = field.first.str();
    bool accepted = false;
    for (const std::string_view candidate : allowed) {
      if (field_name == candidate) {
        accepted = true;
        break;
      }
    }
    if (!accepted) {
      error_message =
        "compile-plan profile contains an unsupported field: " + field_name;
      return false;
    }
  }
  return true;
}

} // namespace

bool
probe_compile_plan_diag_dir(
  const std::filesystem::path& plan_path,
  std::filesystem::path& out_diag_dir
) {
  std::string plan_text;
  std::string error_message;
  if (!read_text_file(plan_path, plan_text, error_message)) {
    return false;
  }

  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(plan_text);
  if (!parsed) {
    return false;
  }
  const llvm::json::Object* root = parsed->getAsObject();
  if (root == nullptr) {
    return false;
  }

  const llvm::json::Object* outputs = root->getObject("outputs");
  if (outputs == nullptr) {
    return false;
  }

  const auto diag_dir = outputs->getString("diag_dir");
  if (!diag_dir.has_value() || diag_dir->empty()) {
    return false;
  }

  const std::filesystem::path candidate{std::string(*diag_dir)};
  if (!candidate.is_absolute()) {
    return false;
  }

  out_diag_dir = candidate;
  return true;
}

bool
parse_compile_plan(
  const std::filesystem::path& plan_path,
  CompilePlanRequest& out_request,
  std::string& error_message
) {
  std::string error_subcode;
  return parse_compile_plan(plan_path, out_request, error_message, error_subcode);
}

bool
parse_compile_plan(
  const std::filesystem::path& plan_path,
  CompilePlanRequest& out_request,
  std::string& error_message,
  std::string& error_subcode
) {
  error_subcode = "compile_plan_invalid";
  std::string plan_text;
  if (!read_text_file(plan_path, plan_text, error_message)) {
    return false;
  }

  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(plan_text);
  if (!parsed) {
    error_message = "compile-plan is not valid JSON: " + llvm::toString(parsed.takeError());
    return false;
  }
  const llvm::json::Object* root = parsed->getAsObject();
  if (root == nullptr) {
    error_message = "compile-plan must be a JSON object";
    return false;
  }

  const llvm::json::Object* generated_by = nullptr;
  const llvm::json::Object* entry = nullptr;
  const llvm::json::Object* toolchain = nullptr;
  const llvm::json::Object* profile = nullptr;
  const llvm::json::Object* resolution = nullptr;
  const llvm::json::Object* outputs = nullptr;
  const llvm::json::Object* emit = nullptr;
  const llvm::json::Array* packages = nullptr;
  std::int64_t plan_version = 0;

  if (!json_require_integer(*root, "plan_version", plan_version, error_message)
      || !json_require_object(*root, "generated_by", generated_by, error_message)
      || !json_require_string(*root, "intent", out_request.intent, error_message, "intent")
      || !compile_plan_require_absolute_path(*root, "workspace_root", out_request.workspace_root, error_message)
      || !json_require_object(*root, "entry", entry, error_message)
      || !json_require_object(*root, "toolchain", toolchain, error_message)
      || !json_require_object(*root, "profile", profile, error_message)
      || !json_require_array(*root, "packages", packages, error_message)
      || !json_require_object(*root, "resolution", resolution, error_message)
      || !json_require_object(*root, "outputs", outputs, error_message)
      || !json_require_object(*root, "emit", emit, error_message)) {
    return false;
  }

  out_request.plan_version = static_cast<int>(plan_version);
  out_request.plan_path = plan_path;

  std::string generated_by_version;
  std::string profile_name;
  if (!json_require_string(*generated_by, "tool", out_request.generated_by_tool, error_message, "generated_by.tool")
      || !json_require_string(*generated_by, "version", generated_by_version, error_message, "generated_by.version")
      || !json_require_string(*profile, "name", profile_name, error_message, "profile.name")) {
    return false;
  }
  (void) generated_by_version;
  (void) profile_name;
  // Pafio is the only ecosystem project-plan producer. The "styio" value is
  // reserved for the compiler's direct single-file `styio build` path, which
  // reuses this parser internally and does not represent a project workflow.
  if (!(out_request.generated_by_tool == "pafio" || out_request.generated_by_tool == "styio")) {
    if (emit->get("observable_static_snapshot") != nullptr) {
      error_subcode = "observable_static_snapshot_styio_produced";
      error_message =
        "observable static snapshots require a Pafio-produced compile plan";
    } else {
      error_message = "compile-plan generated_by.tool must equal \"pafio\" or \"styio\"";
    }
    return false;
  }
  if (!compile_plan_profile_has_only_v1_fields(*profile, error_message)) {
    return false;
  }

  if (!(out_request.intent == "build"
        || out_request.intent == "check"
        || out_request.intent == "run"
        || out_request.intent == "test")) {
    error_message = "unsupported compile-plan intent: " + out_request.intent;
    return false;
  }
  if (packages->empty()) {
    error_message = "compile-plan packages array must not be empty";
    return false;
  }

  if (!json_require_string(*entry, "package_id", out_request.entry_package_id, error_message, "entry.package_id")
      || !json_require_string(*entry, "target_kind", out_request.entry_target_kind, error_message, "entry.target_kind")
      || !json_require_string(*entry, "target_name", out_request.entry_target_name, error_message, "entry.target_name")
      || !compile_plan_require_absolute_path(
        *entry,
        "file",
        out_request.entry_file,
        error_message)) {
    return false;
  }
  if (!(out_request.entry_target_kind == "lib"
        || out_request.entry_target_kind == "bin"
        || out_request.entry_target_kind == "test")) {
    error_message = "unsupported compile-plan entry.target_kind: " + out_request.entry_target_kind;
    return false;
  }
  if (!compile_plan_validate_packages(*packages, out_request.entry_package_id, error_message)) {
    if (emit->get("observable_static_snapshot") != nullptr) {
      error_subcode = "observable_static_snapshot_unmatched";
    }
    return false;
  }

  if (!compile_plan_require_absolute_path(
        *outputs,
        "build_root",
        out_request.build_root,
        error_message,
        "outputs.build_root")
      || !compile_plan_require_absolute_path(
        *outputs,
        "artifact_dir",
        out_request.artifact_dir,
        error_message)
      || !compile_plan_require_absolute_path(
        *outputs,
        "diag_dir",
        out_request.diag_dir,
        error_message,
        "outputs.diag_dir")) {
    return false;
  }

  if (!json_require_string(*emit, "error_format", out_request.error_format, error_message, "emit.error_format")
      || !json_require_bool(*emit, "ast", out_request.emit_ast, error_message)
      || !json_require_bool(*emit, "styio_ir", out_request.emit_styio_ir, error_message)
      || !json_require_bool(*emit, "llvm_ir", out_request.emit_llvm_ir, error_message)) {
    return false;
  }
  if (!(out_request.error_format == "text" || out_request.error_format == "jsonl")) {
    error_message = "unsupported compile-plan emit.error_format: " + out_request.error_format;
    return false;
  }

  if (!parse_observable_static_snapshot_request(
        *emit, out_request, error_message, error_subcode)) {
    return false;
  }
  if (!parse_runtime_observation_request(
        *emit, out_request, error_message, error_subcode)) {
    return false;
  }
  if ((out_request.emit_observable_static_snapshot || out_request.emit_runtime_observation)
      && !admit_qualified_compilation_unit(
        *packages,
        out_request.generated_by_tool,
        out_request.entry_package_id,
        out_request.entry_file,
        out_request,
        error_message,
        error_subcode)) {
    return false;
  }

  return true;
}

} // namespace styio::config
