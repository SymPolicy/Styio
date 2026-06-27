#include "CxxReferenceEquivalence.hpp"

#include "StyioPlatform/Platform.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif

#ifndef STYIO_COMPILER_EXE
#define STYIO_COMPILER_EXE ""
#endif

namespace fs = std::filesystem;

namespace styio::testing::algorithms {
namespace {

std::string
compiler_path() {
  const char* from_env = std::getenv("STYIO_COMPILER_EXE");
  if (from_env != nullptr && from_env[0] != '\0') {
    return from_env;
  }
  return STYIO_COMPILER_EXE;
}

fs::path
make_temp_dir() {
  static std::atomic<unsigned long long> counter{0};
  const auto ticks =
    std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path path = fs::temp_directory_path() /
    ("styio_cpp_reference_equivalence_" + std::to_string(ticks) + "_" +
     std::to_string(counter.fetch_add(1)));
  fs::create_directories(path);
  return path;
}

} // namespace

fs::path
source_root() {
  return fs::path(STYIO_SOURCE_DIR);
}

fs::path
case_dir(std::string_view case_name) {
  return source_root() / "tests" / "algorithms" / std::string(case_name);
}

fs::path
styio_program(std::string_view case_name, std::string_view file_name) {
  return case_dir(case_name) / std::string(file_name);
}

std::string
read_text_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string
normalize_newlines(std::string text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
      continue;
    }
    out.push_back(text[i]);
  }
  return out;
}

std::string
format_i32_list(const std::vector<int>& values) {
  std::ostringstream out;
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ',';
    }
    out << values[i];
  }
  out << ']';
  return out.str();
}

CommandResult
run_styio_program(const fs::path& source, const std::string& stdin_text) {
  CommandResult result;
  const fs::path temp_dir = make_temp_dir();
  const fs::path input_path = temp_dir / "stdin.txt";
  const fs::path stdout_path = temp_dir / "stdout.txt";
  const fs::path stderr_path = temp_dir / "stderr.txt";

  {
    std::ofstream input(input_path, std::ios::binary | std::ios::trunc);
    input << stdin_text;
  }

  const std::string compiler = compiler_path();
  if (compiler.empty()) {
    result.stderr_text = "STYIO_COMPILER_EXE is not configured";
    fs::remove_all(temp_dir);
    return result;
  }

  const styio::platform::ProcessResult process =
    styio::platform::run_process_to_logs(
      {compiler, "--file", source.generic_string()},
      input_path,
      stdout_path,
      stderr_path);
  result.exit_code = process.exit_code;
  result.stdout_text = normalize_newlines(read_text_file(stdout_path));
  result.stderr_text = normalize_newlines(read_text_file(stderr_path));
  if (!process.launch_error.empty()) {
    if (!result.stderr_text.empty() && result.stderr_text.back() != '\n') {
      result.stderr_text.push_back('\n');
    }
    result.stderr_text += process.launch_error;
  }

  fs::remove_all(temp_dir);
  return result;
}

} // namespace styio::testing::algorithms
