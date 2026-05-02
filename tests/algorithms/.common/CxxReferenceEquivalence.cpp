#include "CxxReferenceEquivalence.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif

#ifndef STYIO_COMPILER_EXE
#define STYIO_COMPILER_EXE ""
#endif

namespace fs = std::filesystem;

namespace styio::testing::algorithms {
namespace {

int
decode_wait_status(int status) {
#ifdef _WIN32
  return status;
#else
  if (status == -1) {
    return -1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return status;
#endif
}

std::string
shell_quote(const std::string& value) {
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'') {
      out += "'\\''";
    } else {
      out += ch;
    }
  }
  out += "'";
  return out;
}

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

  const std::string command = shell_quote(compiler) + " --file " +
    shell_quote(source.string()) + " < " + shell_quote(input_path.string()) +
    " 2> " + shell_quote(stderr_path.string());

  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    result.stderr_text = "failed to start styio command";
    fs::remove_all(temp_dir);
    return result;
  }

  char buffer[4096];
  while (fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr) {
    result.stdout_text += buffer;
  }
  result.exit_code = decode_wait_status(pclose(pipe));
  result.stderr_text = read_text_file(stderr_path);

  fs::remove_all(temp_dir);
  return result;
}

} // namespace styio::testing::algorithms
