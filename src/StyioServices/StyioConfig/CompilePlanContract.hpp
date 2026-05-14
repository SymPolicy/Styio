#ifndef STYIO_CONFIG_COMPILE_PLAN_CONTRACT_HPP_
#define STYIO_CONFIG_COMPILE_PLAN_CONTRACT_HPP_

#include <filesystem>
#include <string>
#include <string_view>

namespace styio::config {

struct CompilePlanRequest
{
  std::filesystem::path plan_path;
  int plan_version = 0;
  std::string intent;
  std::string build_mode;
  std::filesystem::path workspace_root;
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
};

std::string_view
default_build_mode_name();

bool
is_supported_build_mode(std::string_view build_mode);

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

} // namespace styio::config

#endif
