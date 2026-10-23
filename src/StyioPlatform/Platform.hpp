#pragma once
#ifndef STYIO_PLATFORM_PLATFORM_HPP_
#define STYIO_PLATFORM_PLATFORM_HPP_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace styio::platform {

using DynamicLibraryHandle = void*;

struct ProcessResult {
  int exit_code = -1;
  std::string launch_error;

  bool ok() const {
    return exit_code == 0 && launch_error.empty();
  }
};

DynamicLibraryHandle load_dynamic_library(
  const std::filesystem::path& path,
  std::string& error_message);
void* lookup_dynamic_symbol(
  DynamicLibraryHandle handle,
  const std::string& name,
  std::string& error_message);
void unload_dynamic_library(DynamicLibraryHandle handle);

ProcessResult run_process_to_log(
  const std::vector<std::string>& argv,
  const std::filesystem::path& log_path,
  bool capture_stdout);
ProcessResult run_process_to_logs(
  const std::vector<std::string>& argv,
  const std::filesystem::path& stdout_log_path,
  const std::filesystem::path& stderr_log_path);
ProcessResult run_process_to_logs(
  const std::vector<std::string>& argv,
  const std::filesystem::path& stdin_path,
  const std::filesystem::path& stdout_log_path,
  const std::filesystem::path& stderr_log_path);

std::filesystem::path create_temp_directory(
  const std::string& prefix,
  std::string& error_message);
std::filesystem::path current_executable_path();
std::filesystem::path current_executable_dir();

const char* executable_suffix();
const char* shared_library_prefix();
const char* shared_library_suffix();
const char* object_suffix();
char path_list_separator();

bool is_executable_file(const std::filesystem::path& path);
std::vector<std::filesystem::path> executable_name_candidates(
  const std::filesystem::path& path);
bool find_executable(const std::string& name, std::string& out_command);

std::uint64_t process_id();

}  // namespace styio::platform

#endif  // STYIO_PLATFORM_PLATFORM_HPP_
