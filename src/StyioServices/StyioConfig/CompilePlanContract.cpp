#include "CompilePlanContract.hpp"

#include <array>
#include <fstream>
#include <sstream>
#include <string_view>

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

  std::string generated_by_tool;
  std::string generated_by_version;
  std::string profile_name;
  if (!json_require_string(*generated_by, "tool", generated_by_tool, error_message, "generated_by.tool")
      || !json_require_string(*generated_by, "version", generated_by_version, error_message, "generated_by.version")
      || !json_require_string(*profile, "name", profile_name, error_message, "profile.name")) {
    return false;
  }
  (void) generated_by_version;
  (void) profile_name;
  if (!(generated_by_tool == "pafio" || generated_by_tool == "styio")) {
    error_message = "compile-plan generated_by.tool must equal \"pafio\" or \"styio\"";
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

  return true;
}

} // namespace styio::config
