#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace styio::testing::algorithms {

struct CommandResult
{
  int exit_code = -1;
  std::string stdout_text;
  std::string stderr_text;
};

std::filesystem::path source_root();
std::filesystem::path case_dir(std::string_view case_name);
std::filesystem::path styio_program(std::string_view case_name, std::string_view file_name);

std::string read_text_file(const std::filesystem::path& path);
std::string format_i32_list(const std::vector<int>& values);
CommandResult run_styio_program(const std::filesystem::path& source, const std::string& stdin_text);

} // namespace styio::testing::algorithms
