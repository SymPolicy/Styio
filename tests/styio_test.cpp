#include <gtest/gtest.h>

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#include "StyioTesting/PipelineCheck.hpp"
#include "StyioToken/Token.hpp"

namespace fs = std::filesystem;

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif

#ifndef STYIO_COMPILER_EXE
#define STYIO_COMPILER_EXE ""
#endif

#ifndef STYIO_NANO_COMPILER_EXE
#define STYIO_NANO_COMPILER_EXE ""
#endif

namespace
{

struct CommandResult
{
  int exit_code = -1;
  std::string stdout_text;
};

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

CommandResult
run_stdout_command(const std::string& cmd) {
  CommandResult result;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    return result;
  }
  char buf[4096];
  while (fgets(buf, static_cast<int>(sizeof(buf)), pipe) != nullptr) {
    result.stdout_text += buf;
  }
  const int status = pclose(pipe);
  result.exit_code = decode_wait_status(status);
  return result;
}

std::string
read_text_file_latest(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string
trim_copy_latest(const std::string& text) {
  size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::size_t
count_substring_latest(const std::string& text, const std::string& needle) {
  if (needle.empty()) {
    return 0;
  }
  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

const char*
styio_compiler_runner_latest() {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  return runner;
}

struct CompilePlanContractCaseLatest
{
  fs::path root;
  fs::path source;
  fs::path build_root;
  fs::path artifact_dir;
  fs::path diag_dir;
  fs::path plan_path;
};

CompilePlanContractCaseLatest
write_compile_plan_contract_case_latest(
  const std::string& suffix,
  const std::string& profile_json,
  const std::string& packages_json,
  const std::string& entry_package_id = "demo/app@0.1.0"
) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  CompilePlanContractCaseLatest paths;
  paths.root =
    fs::temp_directory_path() / ("styio-compile-plan-contract-" + suffix + "-" + std::to_string(uniq));
  paths.source = paths.root / "src" / "main.styio";
  paths.build_root = paths.root / ".spio" / "build" / "case";
  paths.artifact_dir = paths.build_root / "artifacts";
  paths.diag_dir = paths.build_root / "diag";
  paths.plan_path = paths.build_root / "plan.json";

  std::error_code ec;
  fs::create_directories(paths.source.parent_path(), ec);
  if (ec) {
    throw std::runtime_error("cannot create compile-plan source directory: " + ec.message());
  }
  fs::create_directories(paths.build_root, ec);
  if (ec) {
    throw std::runtime_error("cannot create compile-plan build directory: " + ec.message());
  }

  {
    std::ofstream out(paths.source);
    if (!out.is_open()) {
      throw std::runtime_error("cannot write compile-plan source fixture");
    }
    out << ">_(\"compile-plan-" << suffix << "\")\n";
  }
  {
    std::ofstream out(paths.plan_path);
    if (!out.is_open()) {
      throw std::runtime_error("cannot write compile-plan fixture");
    }
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << paths.root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"" << entry_package_id << "\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-" << suffix << "\",\n"
      << "    \"file\": \"" << paths.source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": " << profile_json << ",\n"
      << "  \"packages\": " << packages_json << ",\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\"build_root\": \"" << paths.build_root.string() << "\", \"artifact_dir\": \""
      << paths.artifact_dir.string() << "\", \"diag_dir\": \"" << paths.diag_dir.string() << "\"},\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  return paths;
}

void
expect_compile_plan_invalid_with_diag_latest(
  const fs::path& plan_path,
  const fs::path& diag_dir,
  const std::string& expected_message
) {
  const char* runner = styio_compiler_runner_latest();
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"subcode\":\"compile_plan_invalid\""), std::string::npos)
    << result.stdout_text;
  EXPECT_NE(result.stdout_text.find(expected_message), std::string::npos)
    << result.stdout_text;

  const fs::path diag_path = diag_dir / "diagnostics.jsonl";
  ASSERT_TRUE(fs::exists(diag_path));
  const std::string diagnostics = read_text_file_latest(diag_path);
  EXPECT_NE(diagnostics.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(diagnostics.find(expected_message), std::string::npos)
    << diagnostics;
}

std::string
sha256_file_latest(const fs::path& path) {
  const CommandResult shasum =
    run_stdout_command(std::string("shasum -a 256 \"") + path.string() + "\" 2>/dev/null");
  if (shasum.exit_code == 0) {
    std::istringstream in(shasum.stdout_text);
    std::string digest;
    in >> digest;
    if (!digest.empty()) {
      return digest;
    }
  }

  const CommandResult sha256sum =
    run_stdout_command(std::string("sha256sum \"") + path.string() + "\" 2>/dev/null");
  if (sha256sum.exit_code == 0) {
    std::istringstream in(sha256sum.stdout_text);
    std::string digest;
    in >> digest;
    return digest;
  }
  return "";
}

}  // namespace

TEST(StyioFiveLayerPipeline, P01_print_add) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p01_print_add";
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
}

TEST(StyioFiveLayerPipeline, P02_simple_func) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p02_simple_func";
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
}

TEST(StyioFiveLayerPipeline, P03_write_file) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p03_write_file";
  const fs::path output_path("/tmp/styio_pipeline_out.txt");
  const fs::path expected_output =
    case_dir / "expected" / "output_file.txt";
  fs::remove(output_path);

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;

  ASSERT_TRUE(fs::exists(output_path));
  EXPECT_EQ(read_text_file_latest(output_path), read_text_file_latest(expected_output));
  fs::remove(output_path);
}

TEST(StyioFiveLayerPipeline, P04_read_file) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p04_read_file";
  const fs::path input_path("/tmp/styio_pipeline_in.txt");
  const fs::path expected_input =
    case_dir / "expected" / "input_file.txt";
  {
    std::ofstream out(input_path, std::ios::binary | std::ios::trunc);
    out << read_text_file_latest(expected_input);
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
  fs::remove(input_path);
}

TEST(StyioFiveLayerPipeline, P05_snapshot_accum) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p05_snapshot_accum";
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
}

TEST(StyioFiveLayerPipeline, P06_zip_files) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p06_zip_files";
  const fs::path input_a_path("/tmp/styio_zip_a.txt");
  const fs::path input_b_path("/tmp/styio_zip_b.txt");
  const fs::path expected_input_a =
    case_dir / "expected" / "input_a.txt";
  const fs::path expected_input_b =
    case_dir / "expected" / "input_b.txt";
  {
    std::ofstream out(input_a_path, std::ios::binary | std::ios::trunc);
    out << read_text_file_latest(expected_input_a);
  }
  {
    std::ofstream out(input_b_path, std::ios::binary | std::ios::trunc);
    out << read_text_file_latest(expected_input_b);
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
  fs::remove(input_a_path);
  fs::remove(input_b_path);
}

TEST(StyioFiveLayerPipeline, P07_instant_pull) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p07_instant_pull";
  const fs::path input_path("/tmp/styio_pull_value.txt");
  const fs::path expected_input =
    case_dir / "expected" / "input_value.txt";
  {
    std::ofstream out(input_path, std::ios::binary | std::ios::trunc);
    out << read_text_file_latest(expected_input);
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
  fs::remove(input_path);
}

TEST(StyioFiveLayerPipeline, P08_redirect_file) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p08_redirect_file";
  const fs::path output_path("/tmp/styio_pipeline_redirect.txt");
  const fs::path expected_output =
    case_dir / "expected" / "output_file.txt";
  fs::remove(output_path);

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;

  ASSERT_TRUE(fs::exists(output_path));
  EXPECT_EQ(read_text_file_latest(output_path), read_text_file_latest(expected_output));
  fs::remove(output_path);
}

TEST(StyioFiveLayerPipeline, P09_full_pipeline) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p09_full_pipeline";
  const fs::path input_path("/tmp/styio_pipeline_stream_input.txt");
  const fs::path output_path("/tmp/styio_pipeline_stream_output.txt");
  const fs::path expected_input =
    case_dir / "expected" / "input_file.txt";
  const fs::path expected_output =
    case_dir / "expected" / "output_file.txt";
  {
    std::ofstream out(input_path, std::ios::binary | std::ios::trunc);
    out << read_text_file_latest(expected_input);
  }
  fs::remove(output_path);

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;

  ASSERT_TRUE(fs::exists(output_path));
  EXPECT_EQ(read_text_file_latest(output_path), read_text_file_latest(expected_output));
  fs::remove(input_path);
  fs::remove(output_path);
}

TEST(StyioFiveLayerPipeline, P10_auto_detect_read) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p10_auto_detect_read";
  const fs::path input_path("/tmp/styio_pipeline_auto_input.txt");
  const fs::path expected_input =
    case_dir / "expected" / "input_file.txt";
  {
    std::ofstream out(input_path, std::ios::binary | std::ios::trunc);
    out << read_text_file_latest(expected_input);
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
  fs::remove(input_path);
}

TEST(StyioFiveLayerPipeline, P11_pipe_func) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p11_pipe_func";
  const fs::path input_path("/tmp/styio_pipeline_numbers.txt");
  const fs::path expected_input =
    case_dir / "expected" / "input_file.txt";
  {
    std::ofstream out(input_path, std::ios::binary | std::ios::trunc);
    out << read_text_file_latest(expected_input);
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
  fs::remove(input_path);
}

TEST(StyioFiveLayerPipeline, P12_stdin_echo) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p12_stdin_echo";
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
}

TEST(StyioFiveLayerPipeline, P13_stdin_transform) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p13_stdin_transform";
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
}

TEST(StyioFiveLayerPipeline, P14_stdin_pull) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p14_stdin_pull";
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
}

TEST(StyioFiveLayerPipeline, P15_stdin_mixed_output) {
  const fs::path case_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "pipeline_cases" / "p15_stdin_mixed_output";
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
}

TEST(StyioFiveLayerPipeline, StdinAliasAstShowsStringHandleType) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-stdin-alias-type-" + std::to_string(uniq) + ".styio");
  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "s <- @stdin\n";
    out << "s >> #(line) => {\n";
    out << "  >_(line)\n";
    out << "}\n";
  }

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --parser-engine=nightly --styio-ast --file \"" + input.string() + "\"");
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("s : stdin[string]"), std::string::npos);

  fs::remove(input);
}

TEST(StyioFiveLayerPipeline, StandaloneCollectBindFromStdinMaterializesStringList) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-stdin-collect-" + std::to_string(uniq) + ".styio");
  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "lines << @stdin\n";
    out << ">_(lines[0])\n";
    out << ">_(lines.length)\n";
  }

  const std::string cmd =
    std::string("printf 'alpha\\nbeta\\n' | \"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\"";
  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("alpha"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("2"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, MachineInfoJsonReportsStableHandshakeFields) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd = std::string("\"") + runner + "\" --machine-info=json";
  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"tool\":\"styio\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"compiler_version\":\"0.0.1\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"channel\":\"nightly\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"active_integration_phase\":\"compile-plan-live\""), std::string::npos);
  EXPECT_NE(
    result.stdout_text.find("\"supported_contracts\":{\"machine_info\":[1],\"jsonl_diagnostics\":[1],\"syntax_check\":[1],\"compile_plan\":[1],\"runtime_events\":[1]}"),
    std::string::npos
  );
  EXPECT_NE(
    result.stdout_text.find("\"supported_contract_versions\":{\"machine_info\":[1],\"jsonl_diagnostics\":[1],\"syntax_check\":[1],\"compile_plan\":[1],\"runtime_events\":[1]}"),
    std::string::npos
  );
  EXPECT_NE(result.stdout_text.find("\"supported_adapter_modes\":[\"cli\"]"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"feature_flags\":{\"single_file_entry\":true"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"stable_diagnostic_codes\":true"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"syntax_check\":true"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"syntax_check_recovery_diagnostics\":true"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"syntax_check_source_context\":true"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"compile_plan_consumer\":true"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"project_execution_via_compile_plan\":true"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"runtime_event_stream\":true"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"machine_info_json\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"single_file_entry\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"jsonl_diagnostics\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"syntax_check_json\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"syntax_check_recovery_diagnostics\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"syntax_check_source_context\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"stable_diagnostic_codes\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"nano_package_registry_publish_v1\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"dict_impl\":{\"selected\":\"ordered-hash\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"edition_max\":\"2026\""), std::string::npos);
}

TEST(StyioDiagnostics, VersionPrintsCompilerVersion) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd = std::string("\"") + runner + "\" --version";
  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "styio 0.0.1\n");
}

TEST(StyioDiagnostics, SyntaxCheckJsonAcceptsSyntaxWithoutTypeChecking) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path source = fs::temp_directory_path() / ("styio-syntax-check-ok-" + std::to_string(uniq) + ".styio");
  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << "result: i32 := missing_symbol\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" check --syntax --json --file \"" + source.string() + "\" 2>&1";
  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"contract\":\"syntax-check\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"status\":\"ok\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"ok\":true"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"phase\":\"parse\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"parser_engine\":\"nightly\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"diagnostics\":[]"), std::string::npos);

  fs::remove(source);
}

TEST(StyioDiagnostics, SyntaxCheckJsonReportsParseFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path source = fs::temp_directory_path() / ("styio-syntax-check-bad-" + std::to_string(uniq) + ".styio");
  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << "# broken := (a: i32) => a +\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" check --syntax --json --file \"" + source.string() + "\" 2>&1";
  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 3) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"contract\":\"syntax-check\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"status\":\"syntax_error\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"ok\":false"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"phase\":\"parse\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_PARSE_UNEXPECTED_TOKEN\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"notes\":[]"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"line\":"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"column\":"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"source_context\":{"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"line_text\":\"# broken := (a: i32) => a +\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"caret\":\"^\""), std::string::npos);

  fs::remove(source);
}

TEST(StyioDiagnostics, SyntaxCheckJsonReportsMultipleRecoveredParseFailures) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path source =
    fs::temp_directory_path() / ("styio-syntax-check-recovery-" + std::to_string(uniq) + ".styio");
  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << "# broken := (a: i32) => a +\n";
    out << "# also_broken := (b: i32) => b +\n";
    out << "valid: i32 := 1\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" check --syntax --json --file \"" + source.string() + "\" 2>&1";
  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 3) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"status\":\"syntax_error\""), std::string::npos);
  EXPECT_EQ(count_substring_latest(result.stdout_text, "\"code\":\"STYIO_PARSE_UNEXPECTED_TOKEN\""), 2U)
    << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"line_text\":\"# broken := (a: i32) => a +\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"line_text\":\"# also_broken := (b: i32) => b +\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"source_context\":{"), std::string::npos);

  fs::remove(source);
}

TEST(StyioDiagnostics, SyntaxCheckJsonReportsLexFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path source = fs::temp_directory_path() / ("styio-syntax-check-lex-" + std::to_string(uniq) + ".styio");
  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << "x := 1\n";
    out << "/* unterminated\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" check --syntax --json --file \"" + source.string() + "\" 2>&1";
  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 2) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"contract\":\"syntax-check\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"status\":\"lexical_error\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"phase\":\"lex\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_LEX_UNTERMINATED_BLOCK_COMMENT\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"notes\":[]"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"line\":2"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"column\":1"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"offset\":7"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"source_context\":{"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"line_text\":\"/* unterminated\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"caret\":\"^\""), std::string::npos);

  fs::remove(source);
}

TEST(StyioDiagnostics, SyntaxCheckRejectsNonAuthoritativeParserEngine) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path source =
    fs::temp_directory_path() / ("styio-syntax-check-authority-" + std::to_string(uniq) + ".styio");
  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << "result: i32 := 1\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" check --syntax --json --parser-engine legacy --file \""
    + source.string() + "\" 2>&1";
  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"contract\":\"syntax-check\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"status\":\"cli_error\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"phase\":\"service\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_SERVICE_INVALID_ARGUMENT\""), std::string::npos);
  EXPECT_NE(
    result.stdout_text.find("styio check --syntax only accepts the authoritative nightly parser"),
    std::string::npos);
  EXPECT_EQ(result.stdout_text.find("\"parser_engine\":\"legacy\""), std::string::npos);

  fs::remove(source);
}

TEST(StyioDiagnostics, MachineInfoJsonReflectsCliDictImplSelection) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --machine-info=json --dict-impl=linear";
  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"dict_impl\":{\"selected\":\"linear\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"source\":\"cli\""), std::string::npos);
}

TEST(StyioDiagnostics, MachineInfoJsonReflectsCliDictImplAliasSelection) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --machine-info=json --dict-impl=v1";
  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"dict_impl\":{\"selected\":\"linear\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"source\":\"cli\""), std::string::npos);
}

TEST(StyioDiagnostics, SourceBuildInfoJsonReportsOfficialSourceLayoutFields) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd = std::string("\"") + runner + "\" --source-build-info=json";
  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"contract\": \"source-build-info\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"source_layout_version\": 1"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"official_source_origin\": \"https://github.com/eBioRing/Styio.git\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"name\": \"stable\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"branch\": \"stable\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"name\": \"nightly\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"branch\": \"nightly\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"supported_build_modes\": ["), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"compile_plan_profile_contract\": {"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"path\": \"profile.build_mode\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"default_build_mode\": \"minimal\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"minimal\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"helper_script\": \"scripts/source-build-minimal.sh\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"cmake_target\": \"styio\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"syntax_check_consumer\": \"check --syntax --json --file\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"id\": \"std_symbols\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"styio_symbol_core\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"id\": \"runtime\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"styio_runtime_core\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"id\": \"services\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"styio_cli_contract_core\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"styio_ide_core\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"styio_lspd\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"src/StyioServices/StyioCLI/\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"src/StyioServices/StyioIDE/\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"id\": \"macro_prelude\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"path\": \"src/StyioParser/SymbolRegistry.cpp\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"src/StyioPrelude/resources.styio\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"macro_like_symbols\": ["), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"match\""), std::string::npos);
}

TEST(StyioDiagnostics, SourceBuildMinimalHelperScriptPrintsCompilerPath) {
  const fs::path helper = fs::path(STYIO_SOURCE_DIR) / "scripts" / "source-build-minimal.sh";
  ASSERT_TRUE(fs::exists(helper));

  const std::string cmd = std::string("bash \"") + helper.string() + "\" --help";
  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("source-build"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("scripts/source-build-minimal.sh"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("builds only the `styio` target"), std::string::npos);
}

TEST(StyioTypes, F32BuiltinMappingUsesF32InternalName) {
  const StyioDataType f32 = styio_data_type_from_name("f32");
  EXPECT_EQ(f32.option, StyioDataTypeOption::Float);
  EXPECT_EQ(f32.name, "f32");
  EXPECT_EQ(f32.num_of_bit, static_cast<size_t>(32));
}

TEST(StyioTypes, GetMaxTypeNumericPromotionByBitWidth) {
  const StyioDataType f32 = styio_data_type_from_name("f32");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType i32 = styio_data_type_from_name("i32");
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType i128 = styio_data_type_from_name("i128");

  EXPECT_EQ(getMaxType(f32, i32).name, "f32");
  EXPECT_EQ(getMaxType(i32, f32).name, "f32");
  EXPECT_EQ(getMaxType(f64, i32).name, "f64");
  EXPECT_EQ(getMaxType(f32, i64).name, "f64");
  EXPECT_EQ(getMaxType(f32, f64).name, "f64");
  EXPECT_EQ(getMaxType(i32, i64).name, "i64");

  const StyioDataType untyped_int{StyioDataTypeOption::Integer, "int", 0};
  EXPECT_EQ(getMaxType(untyped_int, untyped_int).name, "int");

  const StyioDataType promoted = getMaxType(f32, i128);
  EXPECT_EQ(promoted.option, StyioDataTypeOption::Float);
  EXPECT_EQ(promoted.name, "f64");
  EXPECT_EQ(promoted.num_of_bit, static_cast<size_t>(64));
}

TEST(StyioDiagnostics, CompilePlanBuildWritesArtifactsWithoutExecutingEntry) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-build-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-build\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"build_mode\": \"minimal\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text.find("compile-plan-build"), std::string::npos);
  ASSERT_TRUE(fs::exists(artifact_dir / "demo.llvm.ir"));
  ASSERT_TRUE(fs::exists(build_root / "receipt.json"));
  ASSERT_TRUE(fs::exists(diag_dir / "diagnostics.jsonl"));
  ASSERT_TRUE(fs::exists(build_root / "runtime-events.jsonl"));
  const std::string receipt = read_text_file_latest(build_root / "receipt.json");
  const std::string runtime_events = read_text_file_latest(build_root / "runtime-events.jsonl");
  EXPECT_NE(receipt.find("\"build_mode\":\"minimal\""), std::string::npos);
  EXPECT_NE(receipt.find("\"executed\":false"), std::string::npos);
  EXPECT_NE(receipt.find("\"session_id\":\""), std::string::npos);
  EXPECT_NE(receipt.find("\"runtime_events_path\":\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"build_mode\":\"minimal\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"compile.started\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.entered\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.exited\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"transition.fired\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"state.changed\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"compile.finished\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"unit_id\":\"demo/app@0.1.0::bin:demo\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"final_phase\":\"codegen_ready\""), std::string::npos);
  EXPECT_EQ(runtime_events.find("\"eventKind\":\"run.started\""), std::string::npos);
  EXPECT_EQ(runtime_events.find("\"eventKind\":\"thread.spawned\""), std::string::npos);
  EXPECT_EQ(runtime_events.find("\"eventKind\":\"log.emitted\""), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanCheckWritesArtifactsWithoutExecutingEntry) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-check-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-check\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"check\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-check\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"build_mode\": \"minimal\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text.find("compile-plan-check"), std::string::npos);
  ASSERT_TRUE(fs::exists(artifact_dir / "demo-check.llvm.ir"));
  ASSERT_TRUE(fs::exists(build_root / "receipt.json"));
  ASSERT_TRUE(fs::exists(diag_dir / "diagnostics.jsonl"));
  ASSERT_TRUE(fs::exists(build_root / "runtime-events.jsonl"));
  const std::string receipt = read_text_file_latest(build_root / "receipt.json");
  const std::string runtime_events = read_text_file_latest(build_root / "runtime-events.jsonl");
  EXPECT_NE(receipt.find("\"intent\":\"check\""), std::string::npos);
  EXPECT_NE(receipt.find("\"build_mode\":\"minimal\""), std::string::npos);
  EXPECT_NE(receipt.find("\"executed\":false"), std::string::npos);
  EXPECT_NE(receipt.find("\"session_id\":\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"build_mode\":\"minimal\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"compile.started\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.entered\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.exited\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"transition.fired\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"state.changed\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"compile.finished\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"unit_id\":\"demo/app@0.1.0::bin:demo-check\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"final_phase\":\"codegen_ready\""), std::string::npos);
  EXPECT_EQ(runtime_events.find("\"eventKind\":\"run.started\""), std::string::npos);
  EXPECT_EQ(runtime_events.find("\"eventKind\":\"thread.spawned\""), std::string::npos);
  EXPECT_EQ(runtime_events.find("\"eventKind\":\"log.emitted\""), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanRunExecutesAndWritesReceiptAndRequestedArtifacts) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-run-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-run\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"run\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-run\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"build_mode\": \"minimal\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": true, \"styio_ir\": true, \"llvm_ir\": true}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("compile-plan-run"), std::string::npos);
  ASSERT_TRUE(fs::exists(artifact_dir / "demo-run.original.ast.txt"));
  ASSERT_TRUE(fs::exists(artifact_dir / "demo-run.typed.ast.txt"));
  ASSERT_TRUE(fs::exists(artifact_dir / "demo-run.styio.ir.txt"));
  ASSERT_TRUE(fs::exists(artifact_dir / "demo-run.llvm.ir"));
  ASSERT_TRUE(fs::exists(build_root / "receipt.json"));
  ASSERT_TRUE(fs::exists(diag_dir / "diagnostics.jsonl"));
  ASSERT_TRUE(fs::exists(build_root / "runtime-events.jsonl"));
  const std::string receipt = read_text_file_latest(build_root / "receipt.json");
  const std::string runtime_events = read_text_file_latest(build_root / "runtime-events.jsonl");
  EXPECT_NE(receipt.find("\"build_mode\":\"minimal\""), std::string::npos);
  EXPECT_NE(receipt.find("\"executed\":true"), std::string::npos);
  EXPECT_NE(receipt.find("\"session_id\":\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"build_mode\":\"minimal\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"compile.started\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.entered\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.exited\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"transition.fired\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"state.changed\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"compile.finished\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"unit_id\":\"demo/app@0.1.0::bin:demo-run\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"final_phase\":\"executed\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"run.started\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"thread.spawned\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"thread.exited\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"log.emitted\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"stream\":\"stdout\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"message\":\"compile-plan-run\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"run.finished\""), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanTestExecutesAndPublishesUnitTestRuntimeEvents) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-test-" + std::to_string(uniq));
  const fs::path source = root / "tests" / "smoke.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-test\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"test\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"test\",\n"
      << "    \"target_name\": \"smoke\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"build_mode\": \"minimal\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("compile-plan-test"), std::string::npos);
  ASSERT_TRUE(fs::exists(artifact_dir / "smoke.llvm.ir"));
  ASSERT_TRUE(fs::exists(build_root / "receipt.json"));
  ASSERT_TRUE(fs::exists(diag_dir / "diagnostics.jsonl"));
  ASSERT_TRUE(fs::exists(build_root / "runtime-events.jsonl"));
  const std::string receipt = read_text_file_latest(build_root / "receipt.json");
  const std::string runtime_events = read_text_file_latest(build_root / "runtime-events.jsonl");
  EXPECT_NE(receipt.find("\"intent\":\"test\""), std::string::npos);
  EXPECT_NE(receipt.find("\"build_mode\":\"minimal\""), std::string::npos);
  EXPECT_NE(receipt.find("\"executed\":true"), std::string::npos);
  EXPECT_NE(runtime_events.find("\"build_mode\":\"minimal\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.entered\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.test.started\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.test.finished\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"run.started\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"run.finished\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"unit_id\":\"demo/app@0.1.0::test:smoke\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"test_name\":\"smoke\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"success\":true"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanFailureWritesJsonlDiagnosticIntoDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-diag-" + std::to_string(uniq));
  const fs::path source = root / "src" / "missing-main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-missing\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_NE(result.exit_code, 0);

  const fs::path diag_path = diag_dir / "diagnostics.jsonl";
  const fs::path runtime_events_path = build_root / "runtime-events.jsonl";
  ASSERT_TRUE(fs::exists(diag_path));
  ASSERT_TRUE(fs::exists(runtime_events_path));
  const std::string diagnostics = read_text_file_latest(diag_path);
  const std::string runtime_events = read_text_file_latest(runtime_events_path);
  EXPECT_NE(diagnostics.find("\"severity\":\"error\""), std::string::npos);
  EXPECT_NE(diagnostics.find("\"phase\":\"runtime\""), std::string::npos);
  EXPECT_NE(diagnostics.find("\"code\":\"STYIO_RUNTIME_ERROR\""), std::string::npos);
  EXPECT_NE(diagnostics.find("\"notes\":[]"), std::string::npos);
  EXPECT_NE(diagnostics.find("\"file\":\"" + source.string() + "\""), std::string::npos);
  EXPECT_NE(diagnostics.find("file not found"), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.entered\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"unit.exited\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"unit_id\":\"demo/app@0.1.0::bin:demo-missing\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"diagnostic.emitted\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"state.changed\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"to\":\"failed\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"eventKind\":\"compile.failed\""), std::string::npos);
  EXPECT_NE(runtime_events.find("\"final_phase\":\"failed\""), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanInvalidIntentReportsCliDiagnosticAndWritesDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-invalid-intent-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-invalid-intent\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"ship\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-invalid-intent\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"subcode\":\"compile_plan_invalid\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("unsupported compile-plan intent: ship"), std::string::npos);

  const fs::path diag_path = diag_dir / "diagnostics.jsonl";
  ASSERT_TRUE(fs::exists(diag_path));
  const std::string diagnostics = read_text_file_latest(diag_path);
  EXPECT_NE(diagnostics.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(diagnostics.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(diagnostics.find("\"subcode\":\"compile_plan_invalid\""), std::string::npos);
  EXPECT_NE(diagnostics.find("unsupported compile-plan intent: ship"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanInvalidBuildModeReportsCliDiagnosticAndWritesDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-invalid-build-mode-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-invalid-build-mode\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-invalid-build-mode\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"build_mode\": \"full\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"subcode\":\"compile_plan_invalid\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("unsupported compile-plan profile.build_mode: full"), std::string::npos);

  const fs::path diag_path = diag_dir / "diagnostics.jsonl";
  ASSERT_TRUE(fs::exists(diag_path));
  const std::string diagnostics = read_text_file_latest(diag_path);
  EXPECT_NE(diagnostics.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(diagnostics.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(diagnostics.find("\"subcode\":\"compile_plan_invalid\""), std::string::npos);
  EXPECT_NE(diagnostics.find("unsupported compile-plan profile.build_mode: full"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanCliConflictReportsCliDiagnosticAndWritesDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-cli-conflict-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-cli-conflict\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-cli-conflict\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(
      std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" --file \"" + source.string()
      + "\" 2>&1"
    );
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_CLI_CONFLICT\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"subcode\":\"compile_plan_cli_conflict\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("--compile-plan and --file are mutually exclusive"), std::string::npos);

  const fs::path diag_path = diag_dir / "diagnostics.jsonl";
  ASSERT_TRUE(fs::exists(diag_path));
  const std::string diagnostics = read_text_file_latest(diag_path);
  EXPECT_NE(diagnostics.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_CLI_CONFLICT\""), std::string::npos);
  EXPECT_NE(diagnostics.find("\"subcode\":\"compile_plan_cli_conflict\""), std::string::npos);
  EXPECT_NE(diagnostics.find("--compile-plan and --file are mutually exclusive"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanInvalidJsonReportsMachineReadableCliDiagnostic) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-invalid-json-" + std::to_string(uniq));
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out << "{ \"plan_version\": 1,\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"subcode\":\"compile_plan_invalid\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("compile-plan is not valid JSON"), std::string::npos);
  EXPECT_FALSE(fs::exists(build_root / "diag" / "diagnostics.jsonl"));

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanGeneratedByMismatchReportsCliDiagnosticAndWritesDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-generated-by-mismatch-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-generated-by-mismatch\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"other-tool\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-generated-by-mismatch\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("compile-plan generated_by.tool must equal \\\"spio\\\""), std::string::npos);

  const fs::path diag_path = diag_dir / "diagnostics.jsonl";
  ASSERT_TRUE(fs::exists(diag_path));
  const std::string diagnostics = read_text_file_latest(diag_path);
  EXPECT_NE(diagnostics.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(diagnostics.find("compile-plan generated_by.tool must equal \\\"spio\\\""), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanUnsupportedTargetKindReportsCliDiagnosticAndWritesDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-target-kind-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-bad-target-kind\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bench\",\n"
      << "    \"target_name\": \"demo-bad-target-kind\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("unsupported compile-plan entry.target_kind: bench"), std::string::npos);

  const fs::path diag_path = diag_dir / "diagnostics.jsonl";
  ASSERT_TRUE(fs::exists(diag_path));
  const std::string diagnostics = read_text_file_latest(diag_path);
  EXPECT_NE(diagnostics.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(diagnostics.find("unsupported compile-plan entry.target_kind: bench"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanRelativeWorkspaceRootReportsCliDiagnosticAndWritesDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-relative-root-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-relative-root\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"relative-root\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-relative-root\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\n"
      << "    \"build_root\": \"" << build_root.string() << "\",\n"
      << "    \"artifact_dir\": \"" << artifact_dir.string() << "\",\n"
      << "    \"diag_dir\": \"" << diag_dir.string() << "\"\n"
      << "  },\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("compile-plan path must be absolute: workspace_root"), std::string::npos);

  const fs::path diag_path = diag_dir / "diagnostics.jsonl";
  ASSERT_TRUE(fs::exists(diag_path));
  const std::string diagnostics = read_text_file_latest(diag_path);
  EXPECT_NE(diagnostics.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(diagnostics.find("compile-plan path must be absolute: workspace_root"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanMissingOutputsReportsMachineReadableCliDiagnosticWithoutDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-missing-outputs-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-missing-outputs\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-missing-outputs\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_SERVICE_COMPILE_PLAN_INVALID\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("compile-plan is missing required object field: outputs"), std::string::npos);
  EXPECT_FALSE(fs::exists(build_root / "diag" / "diagnostics.jsonl"));

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanUnsupportedVersionWritesCliDiagnosticToDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-version-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-unsupported-version\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 9,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-version\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\"build_root\": \"" << build_root.string() << "\", \"artifact_dir\": \"" << artifact_dir.string()
      << "\", \"diag_dir\": \"" << diag_dir.string() << "\"},\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("unsupported compile-plan version: 9"), std::string::npos);
  ASSERT_TRUE(fs::exists(diag_dir / "diagnostics.jsonl"));
  const std::string diagnostics = read_text_file_latest(diag_dir / "diagnostics.jsonl");
  EXPECT_NE(diagnostics.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(diagnostics.find("unsupported compile-plan version: 9"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanEmptyPackagesWritesCliDiagnosticToDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-empty-packages-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-empty-packages\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-empty-packages\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\"build_root\": \"" << build_root.string() << "\", \"artifact_dir\": \"" << artifact_dir.string()
      << "\", \"diag_dir\": \"" << diag_dir.string() << "\"},\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("compile-plan packages array must not be empty"), std::string::npos);
  ASSERT_TRUE(fs::exists(diag_dir / "diagnostics.jsonl"));
  const std::string diagnostics = read_text_file_latest(diag_dir / "diagnostics.jsonl");
  EXPECT_NE(diagnostics.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(diagnostics.find("compile-plan packages array must not be empty"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanMalformedBuildModeTypeWritesCliDiagnosticToDiagDir) {
  const CompilePlanContractCaseLatest plan = write_compile_plan_contract_case_latest(
    "malformed-build-mode-type",
    "{\"name\": \"dev\", \"build_mode\": false, \"opt_level\": 0, \"debug\": true, \"lto\": false}",
    "[{\"id\": \"demo/app@0.1.0\"}]");

  expect_compile_plan_invalid_with_diag_latest(
    plan.plan_path,
    plan.diag_dir,
    "compile-plan optional string field must be a non-empty string: profile.build_mode");

  fs::remove_all(plan.root);
}

TEST(StyioDiagnostics, CompilePlanMalformedPackageEntryWritesCliDiagnosticToDiagDir) {
  const CompilePlanContractCaseLatest plan = write_compile_plan_contract_case_latest(
    "malformed-package-entry",
    "{\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false}",
    "[42]");

  expect_compile_plan_invalid_with_diag_latest(
    plan.plan_path,
    plan.diag_dir,
    "compile-plan packages[0] must be an object");

  fs::remove_all(plan.root);
}

TEST(StyioDiagnostics, CompilePlanEntryPackageMissingWritesCliDiagnosticToDiagDir) {
  const CompilePlanContractCaseLatest plan = write_compile_plan_contract_case_latest(
    "missing-entry-package",
    "{\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false}",
    "[{\"id\": \"demo/lib@0.1.0\"}]");

  expect_compile_plan_invalid_with_diag_latest(
    plan.plan_path,
    plan.diag_dir,
    "compile-plan entry.package_id is not present in packages: demo/app@0.1.0");

  fs::remove_all(plan.root);
}

TEST(StyioDiagnostics, CompilePlanUnsupportedErrorFormatWritesCliDiagnosticToDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-error-format-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-error-format\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-error-format\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\"build_root\": \"" << build_root.string() << "\", \"artifact_dir\": \"" << artifact_dir.string()
      << "\", \"diag_dir\": \"" << diag_dir.string() << "\"},\n"
      << "  \"emit\": {\"error_format\": \"yaml\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("unsupported compile-plan emit.error_format: yaml"), std::string::npos);
  ASSERT_TRUE(fs::exists(diag_dir / "diagnostics.jsonl"));
  const std::string diagnostics = read_text_file_latest(diag_dir / "diagnostics.jsonl");
  EXPECT_NE(diagnostics.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(diagnostics.find("unsupported compile-plan emit.error_format: yaml"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanRelativeEntryFileWritesCliDiagnosticToDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-relative-entry-file-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-relative-entry-file\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-relative-entry-file\",\n"
      << "    \"file\": \"src/main.styio\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\"build_root\": \"" << build_root.string() << "\", \"artifact_dir\": \"" << artifact_dir.string()
      << "\", \"diag_dir\": \"" << diag_dir.string() << "\"},\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("compile-plan path must be absolute: file"), std::string::npos);
  ASSERT_TRUE(fs::exists(diag_dir / "diagnostics.jsonl"));
  const std::string diagnostics = read_text_file_latest(diag_dir / "diagnostics.jsonl");
  EXPECT_NE(diagnostics.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(diagnostics.find("compile-plan path must be absolute: file"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanRelativeArtifactDirWritesCliDiagnosticToDiagDir) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-relative-artifact-dir-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path diag_dir = build_root / "diag";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-relative-artifact-dir\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-relative-artifact-dir\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\"build_root\": \"" << build_root.string() << "\", \"artifact_dir\": \"artifacts\", \"diag_dir\": \""
      << diag_dir.string() << "\"},\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("compile-plan path must be absolute: artifact_dir"), std::string::npos);
  ASSERT_TRUE(fs::exists(diag_dir / "diagnostics.jsonl"));
  const std::string diagnostics = read_text_file_latest(diag_dir / "diagnostics.jsonl");
  EXPECT_NE(diagnostics.find("\"category\":\"CliError\""), std::string::npos);
  EXPECT_NE(diagnostics.find("compile-plan path must be absolute: artifact_dir"), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioDiagnostics, CompilePlanRelativeDiagDirReportsMachineReadableCliDiagnosticWithoutDiagFile) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-compile-plan-relative-diag-dir-" + std::to_string(uniq));
  const fs::path source = root / "src" / "main.styio";
  const fs::path build_root = root / ".spio" / "build" / "case";
  const fs::path artifact_dir = build_root / "artifacts";
  const fs::path plan_path = build_root / "plan.json";
  ASSERT_TRUE(fs::create_directories(source.parent_path()));
  ASSERT_TRUE(fs::create_directories(build_root));

  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << ">_(\"compile-plan-relative-diag-dir\")\n";
  }
  {
    std::ofstream out(plan_path);
    ASSERT_TRUE(out.is_open());
    out
      << "{\n"
      << "  \"plan_version\": 1,\n"
      << "  \"generated_by\": {\"tool\": \"spio\", \"version\": \"0.1.0-dev\"},\n"
      << "  \"intent\": \"build\",\n"
      << "  \"workspace_root\": \"" << root.string() << "\",\n"
      << "  \"entry\": {\n"
      << "    \"package_id\": \"demo/app@0.1.0\",\n"
      << "    \"target_kind\": \"bin\",\n"
      << "    \"target_name\": \"demo-relative-diag-dir\",\n"
      << "    \"file\": \"" << source.string() << "\"\n"
      << "  },\n"
      << "  \"toolchain\": {\"channel\": \"stable\", \"edition\": \"2026\", \"implicit_std\": true, \"std_package_id\": \"styio/std@2026\"},\n"
      << "  \"profile\": {\"name\": \"dev\", \"opt_level\": 0, \"debug\": true, \"lto\": false},\n"
      << "  \"packages\": [{\"id\": \"demo/app@0.1.0\"}],\n"
      << "  \"resolution\": {\"resolver\": \"single-version-v1\", \"package_order\": [\"demo/app@0.1.0\"]},\n"
      << "  \"outputs\": {\"build_root\": \"" << build_root.string() << "\", \"artifact_dir\": \"" << artifact_dir.string()
      << "\", \"diag_dir\": \"diag\"},\n"
      << "  \"emit\": {\"error_format\": \"jsonl\", \"ast\": false, \"styio_ir\": false, \"llvm_ir\": false}\n"
      << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --compile-plan \"" + plan_path.string() + "\" 2>&1");
  EXPECT_EQ(result.exit_code, 6) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("compile-plan path must be absolute: diag_dir"), std::string::npos);
  EXPECT_FALSE(fs::exists(build_root / "diag" / "diagnostics.jsonl"));

  fs::remove_all(root);
}

TEST(StyioDiagnostics, MachineInfoJsonReflectsProjectConfigAndCliOverride) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path project_dir =
    fs::temp_directory_path() / ("styio-dict-config-" + std::to_string(uniq));
  const fs::path input = project_dir / "main.styio";
  const fs::path config = project_dir / "styio.toml";
  ASSERT_TRUE(fs::create_directories(project_dir));

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << ">_(1)\n";
  }
  {
    std::ofstream out(config);
    ASSERT_TRUE(out.is_open());
    out << "[runtime]\n";
    out << "dict_impl = \"linear\"\n";
  }

  const std::string base_cmd =
    std::string("cd \"") + project_dir.string() + "\" && \"" + runner
    + "\" --machine-info=json --file \"" + input.filename().string() + "\"";
  const CommandResult project_selected = run_stdout_command(base_cmd);
  ASSERT_EQ(project_selected.exit_code, 0) << project_selected.stdout_text;
  EXPECT_NE(project_selected.stdout_text.find("\"dict_impl\":{\"selected\":\"linear\""), std::string::npos);
  EXPECT_NE(project_selected.stdout_text.find("\"source\":\"project-config\""), std::string::npos);
  EXPECT_NE(project_selected.stdout_text.find("\"config_file\":\""), std::string::npos);

  const CommandResult cli_override =
    run_stdout_command(base_cmd + " --dict-impl=ordered-hash");
  ASSERT_EQ(cli_override.exit_code, 0) << cli_override.stdout_text;
  EXPECT_NE(cli_override.stdout_text.find("\"dict_impl\":{\"selected\":\"ordered-hash\""), std::string::npos);
  EXPECT_NE(cli_override.stdout_text.find("\"source\":\"cli\""), std::string::npos);

  fs::remove_all(project_dir);
}

TEST(StyioNano, MachineInfoReflectsPrunedProfile) {
  const char* runner = std::getenv("STYIO_NANO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_NANO_COMPILER_EXE;
  }
  if (runner == nullptr || runner[0] == '\0') {
    GTEST_SKIP() << "styio-nano target not built";
  }

  const std::string cmd = std::string("\"") + runner + "\" --machine-info=json";
  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"channel\":\"nano\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"variant\":\"nano\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"name\":\"edge-default\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"dict_impl\":{\"selected\":\"ordered-hash\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"supported\":[\"ordered-hash\"]"), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("\"linear\""), std::string::npos);
}

TEST(StyioNano, DisabledFlagsAndBackendsAreRejected) {
  const char* runner = std::getenv("STYIO_NANO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_NANO_COMPILER_EXE;
  }
  if (runner == nullptr || runner[0] == '\0') {
    GTEST_SKIP() << "styio-nano target not built";
  }

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-nano-disabled-flag-" + std::to_string(uniq) + ".styio");
  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << ">_(1)\n";
  }

  const CommandResult shadow_disabled =
    run_stdout_command(std::string("\"") + runner + "\" --parser-shadow-compare --file \"" + input.string() + "\" 2>&1");
  EXPECT_EQ(shadow_disabled.exit_code, 6);
  EXPECT_NE(
    shadow_disabled.stdout_text.find("disabled in this styio-nano profile"),
    std::string::npos
  );

  const CommandResult backend_disabled =
    run_stdout_command(std::string("\"") + runner + "\" --dict-impl=linear --file \"" + input.string() + "\" 2>&1");
  EXPECT_EQ(backend_disabled.exit_code, 6);
  EXPECT_NE(backend_disabled.stdout_text.find("unsupported --dict-impl: linear"), std::string::npos);

  fs::remove(input);
}

TEST(StyioNanoPackage, LocalSubsetConfigMaterializesBundle) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-local-" + std::to_string(uniq));
  const fs::path output_dir = root / "bundle";
  const fs::path config = root / "nano.toml";
  ASSERT_TRUE(fs::create_directories(root));

  {
    std::ofstream out(config);
    ASSERT_TRUE(out.is_open());
    out << "[nano]\n";
    out << "mode = \"local-subset\"\n";
    out << "name = \"edge-local-test\"\n";
    out << "output_dir = \"" << output_dir.string() << "\"\n";
    out << "\n[nano.local]\n";
    out << "profile = \"" << (fs::path(STYIO_SOURCE_DIR) / "configs" / "styio-nano-default.toml").string() << "\"\n";
    out << "source_root = \"" << fs::path(STYIO_SOURCE_DIR).string() << "\"\n";
  }

  const CommandResult created =
    run_stdout_command(std::string("\"") + runner + "\" --nano-create --nano-package-config \"" + config.string() + "\"");
  ASSERT_EQ(created.exit_code, 0) << created.stdout_text;
  ASSERT_TRUE(fs::exists(output_dir / "bin" / "styio-nano"));
  ASSERT_TRUE(fs::exists(output_dir / "styio-nano.profile.toml"));
  ASSERT_TRUE(fs::exists(output_dir / "styio_nano_profile.cmake"));
  ASSERT_TRUE(fs::exists(output_dir / "styio-nano-package.toml"));
  ASSERT_TRUE(fs::exists(output_dir / "build-styio-nano.sh"));
  ASSERT_TRUE(fs::exists(output_dir / "CMakeLists.txt"));
  ASSERT_TRUE(fs::exists(output_dir / "source-closure-manifest.txt"));
  ASSERT_TRUE(fs::exists(output_dir / "src" / "main.cpp"));
  EXPECT_NE(read_text_file_latest(output_dir / "styio-nano-package.toml").find("mode = \"local-subset\""), std::string::npos);
  EXPECT_NE(read_text_file_latest(output_dir / "styio-nano-package.toml").find("version = \"0.0.1\""), std::string::npos);
  EXPECT_NE(read_text_file_latest(output_dir / "source-closure-manifest.txt").find("src/main.cpp"), std::string::npos);

  fs::remove(output_dir / "bin" / "styio-nano");
  const CommandResult rebuilt =
    run_stdout_command(std::string("\"") + (output_dir / "build-styio-nano.sh").string() + "\"");
  ASSERT_EQ(rebuilt.exit_code, 0) << rebuilt.stdout_text;
  ASSERT_TRUE(fs::exists(output_dir / "bin" / "styio-nano"));

  const CommandResult packaged =
    run_stdout_command(std::string("\"") + (output_dir / "bin" / "styio-nano").string() + "\" --machine-info=json");
  ASSERT_EQ(packaged.exit_code, 0) << packaged.stdout_text;
  EXPECT_NE(packaged.stdout_text.find("\"variant\":\"nano\""), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioNanoPackage, LocalSubsetCliMaterializesBundle) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-cli-" + std::to_string(uniq));
  const fs::path output_dir = root / "bundle";
  ASSERT_TRUE(fs::create_directories(root));

  const fs::path profile = fs::path(STYIO_SOURCE_DIR) / "configs" / "styio-nano-default.toml";
  const std::string cmd =
    std::string("\"") + runner + "\" --nano-create --nano-mode=local-subset --nano-name=edge-cli-test"
    + " --nano-output \"" + output_dir.string() + "\""
    + " --nano-profile \"" + profile.string() + "\""
    + " --nano-source-root \"" + fs::path(STYIO_SOURCE_DIR).string() + "\"";
  const CommandResult created = run_stdout_command(cmd);
  ASSERT_EQ(created.exit_code, 0) << created.stdout_text;
  ASSERT_TRUE(fs::exists(output_dir / "bin" / "styio-nano"));
  EXPECT_NE(read_text_file_latest(output_dir / "styio-nano-package.toml").find("name = \"edge-cli-test\""), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioNanoPackage, CloudRepositoryConfigMaterializesBundle) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const char* nano_runner = std::getenv("STYIO_NANO_COMPILER_EXE");
  if (nano_runner == nullptr || nano_runner[0] == '\0') {
    nano_runner = STYIO_NANO_COMPILER_EXE;
  }
  if (nano_runner == nullptr || nano_runner[0] == '\0') {
    GTEST_SKIP() << "styio-nano target not built";
  }

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-cloud-" + std::to_string(uniq));
  const fs::path repo_dir = root / "repo";
  const fs::path marker = repo_dir / "styio-nano-repository.json";
  const fs::path package_root = root / "package";
  const fs::path blob = root / "edge-cloud.tar";
  const fs::path install_dir = root / "install";
  const std::string package_name = "edge/default";
  const std::string version = "0.0.1";
  const fs::path entry =
    repo_dir / "index" / "edge" / "default" / (version + ".json");
  const fs::path config = root / "nano.toml";
  ASSERT_TRUE(fs::create_directories(package_root / "bin"));
  ASSERT_TRUE(fs::create_directories(repo_dir / "index" / "edge" / "default"));

  {
    std::error_code ec;
    fs::copy_file(nano_runner, package_root / "bin" / "styio-nano", fs::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << ec.message();
    fs::copy_file(
      fs::path(STYIO_SOURCE_DIR) / "configs" / "styio-nano-default.toml",
      package_root / "styio-nano.profile.toml",
      fs::copy_options::overwrite_existing,
      ec
    );
    ASSERT_FALSE(ec) << ec.message();
  }
  {
    std::ofstream out(package_root / "styio-nano-package.toml");
    ASSERT_TRUE(out.is_open());
    out << "[package]\nname = \"edge-cloud-test\"\nchannel = \"nano\"\nmode = \"cloud\"\n";
  }
  const CommandResult tar_created =
    run_stdout_command(std::string("tar -cf \"") + blob.string() + "\" -C \"" + package_root.string() + "\" .");
  ASSERT_EQ(tar_created.exit_code, 0) << tar_created.stdout_text;
  const std::string sha256 = trim_copy_latest(sha256_file_latest(blob));
  ASSERT_EQ(sha256.size(), 64U);
  const fs::path blob_dest = repo_dir / "blobs" / "sha256" / sha256.substr(0, 2) / sha256.substr(2, 2) / (sha256 + ".tar");
  ASSERT_TRUE(fs::create_directories(blob_dest.parent_path()));
  {
    std::error_code ec;
    fs::copy_file(blob, blob_dest, fs::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << ec.message();
  }
  {
    std::ofstream out(marker);
    ASSERT_TRUE(out.is_open());
    out << "{\n";
    out << "  \"kind\": \"styio-nano-static\",\n";
    out << "  \"schema_version\": 1\n";
    out << "}\n";
  }
  {
    std::ofstream out(entry);
    ASSERT_TRUE(out.is_open());
    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"package\": \"" << package_name << "\",\n";
    out << "  \"version\": \"" << version << "\",\n";
    out << "  \"channel\": \"nano\",\n";
    out << "  \"sha256\": \"" << sha256 << "\",\n";
    out << "  \"size_bytes\": " << fs::file_size(blob_dest) << ",\n";
    out << "  \"blob_path\": \"" << (fs::path("blobs") / "sha256" / sha256.substr(0, 2) / sha256.substr(2, 2) / (sha256 + ".tar")).generic_string() << "\",\n";
    out << "  \"published_at\": \"2026-04-12T00:00:00Z\"\n";
    out << "}\n";
  }
  {
    std::ofstream out(config);
    ASSERT_TRUE(out.is_open());
    out << "[nano]\n";
    out << "mode = \"cloud\"\n";
    out << "output_dir = \"" << install_dir.string() << "\"\n";
    out << "\n[nano.cloud]\n";
    out << "registry = \"" << repo_dir.string() << "\"\n";
    out << "package = \"" << package_name << "\"\n";
    out << "version = \"" << version << "\"\n";
  }

  const CommandResult created =
    run_stdout_command(std::string("\"") + runner + "\" --nano-create --nano-package-config \"" + config.string() + "\"");
  ASSERT_EQ(created.exit_code, 0) << created.stdout_text;
  ASSERT_TRUE(fs::exists(install_dir / "bin" / "styio-nano"));
  ASSERT_TRUE(fs::exists(install_dir / "styio-nano.profile.toml"));
  EXPECT_NE(read_text_file_latest(install_dir / "styio-nano-package.toml").find("mode = \"cloud\""), std::string::npos);
  EXPECT_NE(read_text_file_latest(install_dir / "styio-nano-package.toml").find("package = \"edge/default\""), std::string::npos);
  EXPECT_NE(read_text_file_latest(install_dir / "styio-nano-package.toml").find("sha256 = \""), std::string::npos);

  const CommandResult packaged =
    run_stdout_command(std::string("\"") + (install_dir / "bin" / "styio-nano").string() + "\" --machine-info=json");
  ASSERT_EQ(packaged.exit_code, 0) << packaged.stdout_text;
  EXPECT_NE(packaged.stdout_text.find("\"variant\":\"nano\""), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioNanoPackage, CloudRepositoryRejectsInvalidMarkerContract) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-bad-marker-" + std::to_string(uniq));
  const fs::path repo_dir = root / "repo";
  const fs::path output_dir = root / "install";
  const fs::path config = root / "nano.toml";
  ASSERT_TRUE(fs::create_directories(repo_dir));

  {
    std::ofstream out(repo_dir / "styio-nano-repository.json");
    ASSERT_TRUE(out.is_open());
    out << "{\n";
    out << "  \"kind\": \"not-styio-nano-static\",\n";
    out << "  \"schema_version\": 1\n";
    out << "}\n";
  }
  {
    std::ofstream out(config);
    ASSERT_TRUE(out.is_open());
    out << "[nano]\n";
    out << "mode = \"cloud\"\n";
    out << "output_dir = \"" << output_dir.string() << "\"\n";
    out << "\n[nano.cloud]\n";
    out << "registry = \"" << repo_dir.string() << "\"\n";
    out << "package = \"edge/default\"\n";
    out << "version = \"0.0.1\"\n";
  }

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --nano-create --nano-package-config \""
                       + config.string() + "\" 2>&1");
  EXPECT_NE(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(
    result.stdout_text.find("nano repository marker does not match the supported styio-nano static repository contract"),
    std::string::npos);

  fs::remove_all(root);
}

TEST(StyioNanoPackage, CloudRepositoryRejectsBlobSha256Mismatch) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-sha-mismatch-" + std::to_string(uniq));
  const fs::path repo_dir = root / "repo";
  const fs::path output_dir = root / "install";
  const fs::path config = root / "nano.toml";
  const std::string package_name = "edge/default";
  const std::string version = "0.0.1";
  const std::string expected_sha(64, '0');
  const fs::path blob_relpath =
    fs::path("blobs") / "sha256" / "00" / "00" / (expected_sha + ".tar");
  const fs::path blob_path = repo_dir / blob_relpath;
  const fs::path entry =
    repo_dir / "index" / "edge" / "default" / (version + ".json");
  ASSERT_TRUE(fs::create_directories(blob_path.parent_path()));
  ASSERT_TRUE(fs::create_directories(entry.parent_path()));

  {
    std::ofstream out(repo_dir / "styio-nano-repository.json");
    ASSERT_TRUE(out.is_open());
    out << "{\n";
    out << "  \"kind\": \"styio-nano-static\",\n";
    out << "  \"schema_version\": 1\n";
    out << "}\n";
  }
  {
    std::ofstream out(blob_path, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    out << "not a real nano tarball\n";
  }
  {
    std::ofstream out(entry);
    ASSERT_TRUE(out.is_open());
    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"package\": \"" << package_name << "\",\n";
    out << "  \"version\": \"" << version << "\",\n";
    out << "  \"channel\": \"nano\",\n";
    out << "  \"sha256\": \"" << expected_sha << "\",\n";
    out << "  \"size_bytes\": " << fs::file_size(blob_path) << ",\n";
    out << "  \"blob_path\": \"" << blob_relpath.generic_string() << "\",\n";
    out << "  \"published_at\": \"2026-05-22T00:00:00Z\"\n";
    out << "}\n";
  }
  {
    std::ofstream out(config);
    ASSERT_TRUE(out.is_open());
    out << "[nano]\n";
    out << "mode = \"cloud\"\n";
    out << "output_dir = \"" << output_dir.string() << "\"\n";
    out << "\n[nano.cloud]\n";
    out << "registry = \"" << repo_dir.string() << "\"\n";
    out << "package = \"" << package_name << "\"\n";
    out << "version = \"" << version << "\"\n";
  }

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --nano-create --nano-package-config \""
                       + config.string() + "\" 2>&1");
  EXPECT_NE(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(
    result.stdout_text.find("nano repository blob sha256 mismatch for edge/default@0.0.1"),
    std::string::npos);

  fs::remove_all(root);
}

TEST(StyioNanoPackage, CloudRepositoryRejectsBlobSizeMismatch) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-size-mismatch-" + std::to_string(uniq));
  const fs::path repo_dir = root / "repo";
  const fs::path output_dir = root / "install";
  const fs::path config = root / "nano.toml";
  const std::string package_name = "edge/default";
  const std::string version = "0.0.1";
  const fs::path entry =
    repo_dir / "index" / "edge" / "default" / (version + ".json");
  ASSERT_TRUE(fs::create_directories(entry.parent_path()));

  {
    std::ofstream out(repo_dir / "styio-nano-repository.json");
    ASSERT_TRUE(out.is_open());
    out << "{\n";
    out << "  \"kind\": \"styio-nano-static\",\n";
    out << "  \"schema_version\": 1\n";
    out << "}\n";
  }

  const fs::path blob_staging = root / "edge-default.tar";
  {
    std::ofstream out(blob_staging, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    out << "not a real nano tarball, but it has a stable digest\n";
  }
  const std::string sha256 = trim_copy_latest(sha256_file_latest(blob_staging));
  ASSERT_EQ(sha256.size(), 64U);
  const fs::path blob_relpath =
    fs::path("blobs") / "sha256" / sha256.substr(0, 2) / sha256.substr(2, 2) / (sha256 + ".tar");
  const fs::path blob_path = repo_dir / blob_relpath;
  ASSERT_TRUE(fs::create_directories(blob_path.parent_path()));
  {
    std::error_code ec;
    fs::copy_file(blob_staging, blob_path, fs::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << ec.message();
  }

  {
    std::ofstream out(entry);
    ASSERT_TRUE(out.is_open());
    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"package\": \"" << package_name << "\",\n";
    out << "  \"version\": \"" << version << "\",\n";
    out << "  \"channel\": \"nano\",\n";
    out << "  \"sha256\": \"" << sha256 << "\",\n";
    out << "  \"size_bytes\": " << (fs::file_size(blob_path) + 1) << ",\n";
    out << "  \"blob_path\": \"" << blob_relpath.generic_string() << "\",\n";
    out << "  \"published_at\": \"2026-05-22T00:00:00Z\"\n";
    out << "}\n";
  }
  {
    std::ofstream out(config);
    ASSERT_TRUE(out.is_open());
    out << "[nano]\n";
    out << "mode = \"cloud\"\n";
    out << "output_dir = \"" << output_dir.string() << "\"\n";
    out << "\n[nano.cloud]\n";
    out << "registry = \"" << repo_dir.string() << "\"\n";
    out << "package = \"" << package_name << "\"\n";
    out << "version = \"" << version << "\"\n";
  }

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --nano-create --nano-package-config \""
                       + config.string() + "\" 2>&1");
  EXPECT_NE(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(
    result.stdout_text.find("nano repository blob size mismatch for edge/default@0.0.1"),
    std::string::npos);

  fs::remove_all(root);
}

TEST(StyioNanoPackage, PublishConfigWritesRepositoryAndRoundTripsToCloudInstall) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const char* nano_runner = std::getenv("STYIO_NANO_COMPILER_EXE");
  if (nano_runner == nullptr || nano_runner[0] == '\0') {
    nano_runner = STYIO_NANO_COMPILER_EXE;
  }
  if (nano_runner == nullptr || nano_runner[0] == '\0') {
    GTEST_SKIP() << "styio-nano target not built";
  }

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-publish-" + std::to_string(uniq));
  const fs::path package_dir = root / "package";
  const fs::path repo_dir = root / "repo";
  const fs::path install_dir = root / "install";
  const fs::path publish_config = root / "publish.toml";
  const fs::path install_config = root / "install.toml";
  const std::string package_name = "edge/default";
  const std::string version = "0.0.2";
  ASSERT_TRUE(fs::create_directories(package_dir / "bin"));

  {
    std::error_code ec;
    fs::copy_file(nano_runner, package_dir / "bin" / "styio-nano", fs::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << ec.message();
    fs::copy_file(
      fs::path(STYIO_SOURCE_DIR) / "configs" / "styio-nano-default.toml",
      package_dir / "styio-nano.profile.toml",
      fs::copy_options::overwrite_existing,
      ec
    );
    ASSERT_FALSE(ec) << ec.message();
  }
  {
    std::ofstream out(package_dir / "styio-nano-package.toml");
    ASSERT_TRUE(out.is_open());
    out << "[package]\n";
    out << "name = \"edge-default-test\"\n";
    out << "version = \"" << version << "\"\n";
    out << "channel = \"nano\"\n";
    out << "mode = \"local-subset\"\n";
    out << "binary = \"bin/styio-nano\"\n";
    out << "profile = \"styio-nano.profile.toml\"\n";
  }
  {
    std::ofstream out(publish_config);
    ASSERT_TRUE(out.is_open());
    out << "[nano.publish]\n";
    out << "package_dir = \"" << package_dir.string() << "\"\n";
    out << "registry = \"" << repo_dir.string() << "\"\n";
    out << "package = \"" << package_name << "\"\n";
  }

  const CommandResult published =
    run_stdout_command(std::string("\"") + runner + "\" --nano-publish --nano-publish-config \"" + publish_config.string() + "\"");
  ASSERT_EQ(published.exit_code, 0) << published.stdout_text;

  const fs::path marker = repo_dir / "styio-nano-repository.json";
  const fs::path entry = repo_dir / "index" / "edge" / "default" / (version + ".json");
  ASSERT_TRUE(fs::exists(marker));
  ASSERT_TRUE(fs::exists(entry));
  EXPECT_NE(read_text_file_latest(entry).find("\"package\": \"" + package_name + "\""), std::string::npos);
  EXPECT_NE(read_text_file_latest(entry).find("\"version\": \"" + version + "\""), std::string::npos);

  const std::string entry_text = read_text_file_latest(entry);
  const std::string sha_key = "\"sha256\": \"";
  const size_t sha_pos = entry_text.find(sha_key);
  ASSERT_NE(sha_pos, std::string::npos);
  const size_t sha_begin = sha_pos + sha_key.size();
  const size_t sha_end = entry_text.find('"', sha_begin);
  ASSERT_NE(sha_end, std::string::npos);
  const std::string sha256 = entry_text.substr(sha_begin, sha_end - sha_begin);
  ASSERT_EQ(sha256.size(), 64U);
  const fs::path blob_path = repo_dir / "blobs" / "sha256" / sha256.substr(0, 2) / sha256.substr(2, 2) / (sha256 + ".tar");
  ASSERT_TRUE(fs::exists(blob_path));
  EXPECT_EQ(trim_copy_latest(sha256_file_latest(blob_path)), sha256);

  {
    std::ofstream out(install_config);
    ASSERT_TRUE(out.is_open());
    out << "[nano]\n";
    out << "mode = \"cloud\"\n";
    out << "output_dir = \"" << install_dir.string() << "\"\n";
    out << "\n[nano.cloud]\n";
    out << "registry = \"" << repo_dir.string() << "\"\n";
    out << "package = \"" << package_name << "\"\n";
    out << "version = \"" << version << "\"\n";
  }

  const CommandResult installed =
    run_stdout_command(std::string("\"") + runner + "\" --nano-create --nano-package-config \"" + install_config.string() + "\"");
  ASSERT_EQ(installed.exit_code, 0) << installed.stdout_text;
  ASSERT_TRUE(fs::exists(install_dir / "bin" / "styio-nano"));
  EXPECT_NE(read_text_file_latest(install_dir / "styio-nano-package.toml").find("package = \"edge/default\""), std::string::npos);
  EXPECT_NE(read_text_file_latest(install_dir / "styio-nano-package.toml").find("version = \"0.0.2\""), std::string::npos);

  const CommandResult packaged =
    run_stdout_command(std::string("\"") + (install_dir / "bin" / "styio-nano").string() + "\" --machine-info=json");
  ASSERT_EQ(packaged.exit_code, 0) << packaged.stdout_text;
  EXPECT_NE(packaged.stdout_text.find("\"variant\":\"nano\""), std::string::npos);

  fs::remove_all(root);
}

TEST(StyioNanoPackage, PublishRejectsRemoteRegistryRoot) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-remote-publish-" + std::to_string(uniq));
  const fs::path package_dir = root / "package";
  ASSERT_TRUE(fs::create_directories(package_dir / "bin"));
  {
    std::ofstream out(package_dir / "bin" / "styio-nano");
    ASSERT_TRUE(out.is_open());
    out << "#!/bin/sh\n";
  }

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner
                       + "\" --nano-publish --nano-package-dir \"" + package_dir.string()
                       + "\" --nano-registry https://packages.example.invalid/styio"
                       + " --nano-package edge/default --nano-version 0.0.1 2>&1");
  EXPECT_NE(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(
    result.stdout_text.find("styio-nano publish only supports local repository roots or file:// URIs"),
    std::string::npos);

  fs::remove_all(root);
}

TEST(StyioNanoPackage, CloudRepositoryRejectsMalformedEntrySchema) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-bad-entry-" + std::to_string(uniq));
  const fs::path repo_dir = root / "repo";
  const fs::path output_dir = root / "install";
  const fs::path config = root / "nano.toml";
  const std::string package_name = "edge/default";
  const std::string version = "0.0.1";
  const fs::path entry =
    repo_dir / "index" / "edge" / "default" / (version + ".json");
  ASSERT_TRUE(fs::create_directories(entry.parent_path()));

  {
    std::ofstream out(repo_dir / "styio-nano-repository.json");
    ASSERT_TRUE(out.is_open());
    out << "{\n";
    out << "  \"kind\": \"styio-nano-static\",\n";
    out << "  \"schema_version\": 1\n";
    out << "}\n";
  }
  {
    std::ofstream out(entry);
    ASSERT_TRUE(out.is_open());
    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"package\": \"" << package_name << "\",\n";
    out << "  \"version\": \"" << version << "\",\n";
    out << "  \"channel\": \"nano\",\n";
    out << "  \"sha256\": \"" << std::string(64, '0') << "\",\n";
    out << "  \"size_bytes\": \"not-a-number\",\n";
    out << "  \"blob_path\": \"blobs/sha256/00/00/" << std::string(64, '0') << ".tar\"\n";
    out << "}\n";
  }
  {
    std::ofstream out(config);
    ASSERT_TRUE(out.is_open());
    out << "[nano]\n";
    out << "mode = \"cloud\"\n";
    out << "output_dir = \"" << output_dir.string() << "\"\n";
    out << "\n[nano.cloud]\n";
    out << "registry = \"" << repo_dir.string() << "\"\n";
    out << "package = \"" << package_name << "\"\n";
    out << "version = \"" << version << "\"\n";
  }

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --nano-create --nano-package-config \""
                       + config.string() + "\" 2>&1");
  EXPECT_NE(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(
    result.stdout_text.find("nano repository entry is missing a valid size_bytes"),
    std::string::npos);

  fs::remove_all(root);
}

TEST(StyioNanoPackage, CloudManifestRejectsMalformedPackageFile) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-bad-manifest-" + std::to_string(uniq));
  const fs::path output_dir = root / "install";
  const fs::path config = root / "nano.toml";
  const fs::path manifest = root / "manifest.toml";
  ASSERT_TRUE(fs::create_directories(root));

  {
    std::ofstream out(manifest);
    ASSERT_TRUE(out.is_open());
    out << "[package\n";
    out << "name = \"edge-manifest-test\"\n";
  }
  {
    std::ofstream out(config);
    ASSERT_TRUE(out.is_open());
    out << "[nano]\n";
    out << "mode = \"cloud\"\n";
    out << "output_dir = \"" << output_dir.string() << "\"\n";
    out << "\n[nano.cloud]\n";
    out << "manifest = \"" << manifest.string() << "\"\n";
  }

  const CommandResult result =
    run_stdout_command(std::string("\"") + runner + "\" --nano-create --nano-package-config \""
                       + config.string() + "\" 2>&1");
  EXPECT_NE(result.exit_code, 0) << result.stdout_text;
  EXPECT_NE(
    result.stdout_text.find("malformed section header in nano package manifest"),
    std::string::npos);

  fs::remove_all(root);
}

TEST(StyioNanoPackage, CliRejectsMutuallyExclusiveAndMisdirectedOptions) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path root =
    fs::temp_directory_path() / ("styio-nano-package-cli-guards-" + std::to_string(uniq));
  ASSERT_TRUE(fs::create_directories(root));
  const fs::path create_config = root / "create.toml";
  const fs::path publish_config = root / "publish.toml";

  const CommandResult both_modes =
    run_stdout_command(std::string("\"") + runner + "\" --nano-create --nano-publish 2>&1");
  EXPECT_NE(both_modes.exit_code, 0) << both_modes.stdout_text;
  EXPECT_NE(
    both_modes.stdout_text.find("--nano-create and --nano-publish are mutually exclusive"),
    std::string::npos);

  const CommandResult arg_without_mode =
    run_stdout_command(std::string("\"") + runner + "\" --nano-output \"" + root.string() + "\" 2>&1");
  EXPECT_NE(arg_without_mode.exit_code, 0) << arg_without_mode.stdout_text;
  EXPECT_NE(
    arg_without_mode.stdout_text.find("styio-nano packaging arguments require --nano-create or --nano-publish"),
    std::string::npos);

  const CommandResult create_with_publish_only =
    run_stdout_command(std::string("\"") + runner + "\" --nano-create --nano-publish-config \""
                       + publish_config.string() + "\" 2>&1");
  EXPECT_NE(create_with_publish_only.exit_code, 0) << create_with_publish_only.stdout_text;
  EXPECT_NE(
    create_with_publish_only.stdout_text.find("--nano-create does not accept publish-only options"),
    std::string::npos);

  const CommandResult publish_with_create_only =
    run_stdout_command(std::string("\"") + runner + "\" --nano-publish --nano-package-config \""
                       + create_config.string() + "\" 2>&1");
  EXPECT_NE(publish_with_create_only.exit_code, 0) << publish_with_create_only.stdout_text;
  EXPECT_NE(
    publish_with_create_only.stdout_text.find("--nano-publish does not accept create-only options"),
    std::string::npos);

  fs::remove_all(root);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnScalarExpressionsSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions" / "t01_int_arith.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnScalarExpressionsTypedBindSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions" / "t07_typed_bind.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnScalarExpressionsCompoundAssignSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions" / "t14_compound.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnFunctionsSimpleFuncSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "functions" / "t01_simple_func.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnControlFlowMatchExprSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "control_flow" / "t02_match_expr.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnWaveDispatchMergeSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "wave_dispatch" / "t01_wave_merge.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnWaveDispatchSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "wave_dispatch" / "t03_wave_dispatch.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnFileResourcesWriteFileSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "file_resources" / "t02_write_file.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnFileResourcesRedirectSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "file_resources" / "t07_redirect.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnFileResourcesReadFileSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "file_resources" / "t01_read_file.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnFileResourcesAutoDetectSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "file_resources" / "t05_auto_detect.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnFileResourcesPipeFuncSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "file_resources" / "t08_pipe_func.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnStdioOutputBoolSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "stdio_output" / "t04_stdout_bool.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
  EXPECT_EQ(newer.stdout_text, std::string("true\nfalse\n"));
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnStdioOutputFmtStringSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "stdio_output" / "t06_stdout_fmtstr.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
  EXPECT_EQ(newer.stdout_text, std::string("x=10\n"));
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnStreamProcessingInstantPullSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "stream_processing" / "t04_instant_pull.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnStreamProcessingSnapshotSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "stream_processing" / "t03_snapshot.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnStreamProcessingZipCollectionsSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "stream_processing" / "t01_zip_collections.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnStreamProcessingZipUnequalSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "stream_processing" / "t02_zip_unequal.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioStreamZip, MaterializedListHandlesSupportF64AndBool) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-zip-bound-f64-bool-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "rates = [1.5, 2.5, 3.5]\n";
    out << "flags = [true, false]\n";
    out << "rates >> #(r) & flags >> #(flag) => {\n";
    out << "  >_(r)\n";
    out << "  >_(flag)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "1.500000\ntrue\n2.500000\nfalse\n");

  fs::remove(input);
}

TEST(StyioStreamZip, MixedFileAndMaterializedListsTerminateAtShorterInput) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input_a =
    fs::temp_directory_path() / ("styio-zip-list-file-" + std::to_string(uniq) + ".styio");
  const fs::path input_b =
    fs::temp_directory_path() / ("styio-zip-file-list-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input_a);
    ASSERT_TRUE(out.is_open());
    out << "offsets = [1, 2]\n";
    out << "offsets >> #(offset) & @file(\"tests/features/stream_processing/data/prices_a.txt\") >> #(price) => {\n";
    out << "  >_(price + offset)\n";
    out << "}\n";
  }
  {
    std::ofstream out(input_b);
    ASSERT_TRUE(out.is_open());
    out << "names = \"left\nright\nextra\nunused\"\n";
    out << "@file(\"tests/features/stream_processing/data/prices_a.txt\") >> #(price) & names.lines() >> #(name) => {\n";
    out << "  line = name + \" \" + price\n";
    out << "  >_(line)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_a =
    std::string("\"") + runner + "\" --file \"" + input_a.string() + "\" 2>&1";
  const std::string cmd_b =
    std::string("\"") + runner + "\" --file \"" + input_b.string() + "\" 2>&1";

  const CommandResult result_a = run_stdout_command(cmd_a);
  EXPECT_EQ(result_a.exit_code, 0) << result_a.stdout_text;
  EXPECT_EQ(result_a.stdout_text, "101\n202\n");

  const CommandResult result_b = run_stdout_command(cmd_b);
  EXPECT_EQ(result_b.exit_code, 0) << result_b.stdout_text;
  EXPECT_EQ(result_b.stdout_text, "left 100\nright 200\nextra 300\n");

  fs::remove(input_a);
  fs::remove(input_b);
}

TEST(StyioStreamZip, ResourceScalarSelectorFailsClosedAsZipInput) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-zip-resource-scalar-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "@price : i64|..3|\n";
    out << "[10, 20, 30] >> #(v) => {\n";
    out << "  v -> @price\n";
    out << "}\n";
    out << "@price[-1] >> #(p) & [1, 2] >> #(x) => {\n";
    out << "  >_(p + x)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 4);
  EXPECT_NE(result.stdout_text.find("\"category\":\"TypeError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_TYPE_ERROR\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("zip requires iterable inputs on both sides"), std::string::npos);

  fs::remove(input);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnStreamProcessingZipFilesSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "stream_processing" / "t05_zip_files.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnStreamProcessingArbitrageSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "stream_processing" / "t08_arbitrage.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnStreamProcessingFullPipelineSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "stream_processing" / "t10_full_pipeline.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnUntypedParamFunction) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-untyped-param-func-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# id = (x) => x\n";
    out << ">_(id(7))\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 0);
  EXPECT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);

  fs::remove(input);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnHashExprBodyWithoutArrow) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input = fs::temp_directory_path() / ("styio-hash-expr-body-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# mix(a: i32, b: i32) : i32 = a + b\n";
    out << ">_(mix(5, 7))\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 0);
  EXPECT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);

  fs::remove(input);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnHashArrowWithoutAssignment) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-hash-arrow-body-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# const42 : i32 => 42\n";
    out << "# ping => 1\n";
    out << ">_(const42(), ping())\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 0);
  EXPECT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);

  fs::remove(input);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnHashMatchCases) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-hash-match-cases-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# parity(n: i32) ?={\n";
    out << "  0 => 0\n";
    out << "  _ => 1\n";
    out << "}\n";
    out << ">_(parity(0), parity(3))\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 0);
  EXPECT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);

  fs::remove(input);
}

TEST(StyioParserEngine, LegacyAndNightlyMatchOnHashIteratorDefinition) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-hash-iter-def-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# iter_only(x) >> (n) => >_(n)\n";
    out << "iter_only([1, 2, 3])\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --parser-engine=legacy --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 0);
  EXPECT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);

  fs::remove(input);
}

TEST(StyioParserEngine, HashIteratorMatchForwardChainReturnsParseError) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-hash-iter-match-forward-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# iter_only(x) >> (n) ?= 2 => >_(n)\n";
    out << "iter_only([1, 2, 3])\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=legacy --file \""
    + input.string() + "\" 2>&1";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 3);
  EXPECT_EQ(newer.exit_code, 3);
  EXPECT_NE(legacy.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);
  EXPECT_NE(newer.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);
  EXPECT_NE(legacy.stdout_text.find("Styio.ParseError"), std::string::npos);
  EXPECT_NE(newer.stdout_text.find("Styio.ParseError"), std::string::npos);
  EXPECT_EQ(legacy.stdout_text.find("Styio.NotImplemented"), std::string::npos);
  EXPECT_EQ(newer.stdout_text.find("Styio.NotImplemented"), std::string::npos);

  fs::remove(input);
}

TEST(StyioParserEngine, EmptyMatchCasesAreRejectedWithParseError) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-empty-match-cases-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "x = 1\n";
    out << "x ?= {\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=legacy --file \""
    + input.string() + "\" 2>&1";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 3);
  EXPECT_EQ(newer.exit_code, 3);
  EXPECT_NE(legacy.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);
  EXPECT_NE(newer.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);
  EXPECT_EQ(legacy.stdout_text.find("Styio.NotImplemented"), std::string::npos);
  EXPECT_EQ(newer.stdout_text.find("Styio.NotImplemented"), std::string::npos);

  fs::remove(input);
}

TEST(StyioParserEngine, PointerScrutineeMatchDoesNotAbortAndReportsTypeError) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input = fs::temp_directory_path() / ("styio-match-pointer-scrutinee-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "f <- @file(\"tests/features/file_resources/data/input.txt\")\n";
    out << "f >> #(line) => {\n";
    out << "  line ?={\n";
    out << "    1 => >_(1)\n";
    out << "    _ => >_(0)\n";
    out << "  }\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=legacy --file \""
    + input.string() + "\" 2>&1";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);

  EXPECT_EQ(legacy.exit_code, 4);
  EXPECT_EQ(newer.exit_code, 4);
  EXPECT_NE(legacy.stdout_text.find("\"category\":\"TypeError\""), std::string::npos);
  EXPECT_NE(newer.stdout_text.find("\"category\":\"TypeError\""), std::string::npos);
  EXPECT_NE(legacy.stdout_text.find("match scrutinee must be integer-typed"), std::string::npos);
  EXPECT_NE(newer.stdout_text.find("match scrutinee must be integer-typed"), std::string::npos);

  fs::remove(input);
}

TEST(StyioParserEngine, UnsupportedEngineIsRejected) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions" / "t01_int_arith.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_bad =
    std::string("\"") + runner + "\" --parser-engine=bad --file \"" + input.string() + "\" 2>&1";
  const CommandResult bad = run_stdout_command(cmd_bad);
  EXPECT_NE(bad.exit_code, 0);
  EXPECT_NE(bad.stdout_text.find("unsupported --parser-engine"), std::string::npos);
}

TEST(StyioParserEngine, DefaultEngineIsNightlyInShadowArtifact) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions" / "t01_int_arith.styio";
  ASSERT_TRUE(fs::exists(input));

  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-default-engine-" + std::to_string(uniq));
  fs::create_directories(artifact_dir);

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_default_shadow =
    std::string("\"") + runner + "\" --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";
  const CommandResult def = run_stdout_command(cmd_default_shadow);
  ASSERT_EQ(def.exit_code, 0) << def.stdout_text;

  bool found = false;
  bool primary_nightly = false;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    found = true;
    std::ifstream in(entry.path());
    ASSERT_TRUE(in.is_open());
    std::string line;
    std::getline(in, line);
    if (line.find("\"primary_engine\":\"nightly\"") != std::string::npos) {
      primary_nightly = true;
      break;
    }
  }

  EXPECT_TRUE(found);
  EXPECT_TRUE(primary_nightly);
  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, DeprecatedNewAliasMatchesNightlySample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions" / "t01_int_arith.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_alias =
    std::string("\"") + runner + "\" --parser-engine=new --file \"" + input.string() + "\" 2>/dev/null";
  const std::string cmd_nightly =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult alias = run_stdout_command(cmd_alias);
  const CommandResult nightly = run_stdout_command(cmd_nightly);
  ASSERT_EQ(alias.exit_code, 0);
  ASSERT_EQ(nightly.exit_code, 0);
  EXPECT_EQ(alias.stdout_text, nightly.stdout_text);
}

TEST(StyioParserEngine, ShadowCompareAcceptsScalarExpressionsTypedBindSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions" / "t07_typed_bind.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy_shadow =
    std::string("\"") + runner + "\" --parser-engine=legacy --parser-shadow-compare --file \""
    + input.string() + "\" 2>/dev/null";
  const std::string cmd_new_shadow =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --file \""
    + input.string() + "\" 2>/dev/null";

  const CommandResult legacy_shadow = run_stdout_command(cmd_legacy_shadow);
  const CommandResult new_shadow = run_stdout_command(cmd_new_shadow);
  EXPECT_EQ(legacy_shadow.exit_code, 0);
  EXPECT_EQ(new_shadow.exit_code, 0);
}

TEST(StyioParserEngine, ShadowCompareAcceptsScalarExpressionsCoreSuite) {
  const std::vector<std::string> files = {
    "t01_int_arith.styio",
    "t06_binding.styio",
    "t07_typed_bind.styio",
    "t09_multi_print.styio",
    "t14_compound.styio",
  };

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  for (const auto& name : files) {
    const fs::path input = fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions" / name;
    ASSERT_TRUE(fs::exists(input)) << input.string();

    const std::string cmd_legacy_shadow =
      std::string("\"") + runner + "\" --parser-engine=legacy --parser-shadow-compare --file \""
      + input.string() + "\" 2>/dev/null";
    const std::string cmd_new_shadow =
      std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --file \""
      + input.string() + "\" 2>/dev/null";

    const CommandResult legacy_shadow = run_stdout_command(cmd_legacy_shadow);
    const CommandResult new_shadow = run_stdout_command(cmd_new_shadow);
    EXPECT_EQ(legacy_shadow.exit_code, 0) << name;
    EXPECT_EQ(new_shadow.exit_code, 0) << name;
  }
}

TEST(StyioParserEngine, ShadowCompareAcceptsScalarExpressionsFullSuite) {
  const fs::path scalar_expressions_dir =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions";
  ASSERT_TRUE(fs::exists(scalar_expressions_dir));

  std::vector<fs::path> inputs;
  for (const auto& entry : fs::directory_iterator(scalar_expressions_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const fs::path p = entry.path();
    const std::string name = p.filename().string();
    if (p.extension() == ".styio" && name.rfind("t", 0) == 0) {
      inputs.push_back(p);
    }
  }
  std::sort(inputs.begin(), inputs.end());
  ASSERT_FALSE(inputs.empty());

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  for (const auto& input : inputs) {
    const std::string case_name = input.filename().string();
    const std::string cmd_legacy_shadow =
      std::string("\"") + runner + "\" --parser-engine=legacy --parser-shadow-compare --file \""
      + input.string() + "\" 2>/dev/null";
    const std::string cmd_new_shadow =
      std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --file \""
      + input.string() + "\" 2>/dev/null";

    const CommandResult legacy_shadow = run_stdout_command(cmd_legacy_shadow);
    const CommandResult new_shadow = run_stdout_command(cmd_new_shadow);
    EXPECT_EQ(legacy_shadow.exit_code, 0) << case_name;
    EXPECT_EQ(new_shadow.exit_code, 0) << case_name;
  }
}

TEST(StyioParserEngine, ShadowCompareAcceptsFunctionsCoreSuite) {
  const std::vector<std::string> files = {
    "t01_simple_func.styio",
    "t02_typed_return.styio",
    "t03_block_body.styio",
    "t07_no_params.styio",
  };

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  for (const auto& name : files) {
    const fs::path input = fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "functions" / name;
    ASSERT_TRUE(fs::exists(input)) << input.string();

    const std::string cmd_legacy_shadow =
      std::string("\"") + runner + "\" --parser-engine=legacy --parser-shadow-compare --file \""
      + input.string() + "\" 2>/dev/null";
    const std::string cmd_new_shadow =
      std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --file \""
      + input.string() + "\" 2>/dev/null";

    const CommandResult legacy_shadow = run_stdout_command(cmd_legacy_shadow);
    const CommandResult new_shadow = run_stdout_command(cmd_new_shadow);
    EXPECT_EQ(legacy_shadow.exit_code, 0) << name;
    EXPECT_EQ(new_shadow.exit_code, 0) << name;
  }
}

TEST(StyioParserEngine, ShadowCompareAcceptsFunctionsFullSuite) {
  const fs::path functions_dir = fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "functions";
  ASSERT_TRUE(fs::exists(functions_dir));

  std::vector<fs::path> inputs;
  for (const auto& entry : fs::directory_iterator(functions_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const fs::path p = entry.path();
    const std::string name = p.filename().string();
    if (p.extension() == ".styio" && name.rfind("t", 0) == 0) {
      inputs.push_back(p);
    }
  }
  std::sort(inputs.begin(), inputs.end());
  ASSERT_FALSE(inputs.empty());

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  for (const auto& input : inputs) {
    const std::string case_name = input.filename().string();
    const std::string cmd_legacy_shadow =
      std::string("\"") + runner + "\" --parser-engine=legacy --parser-shadow-compare --file \""
      + input.string() + "\" 2>/dev/null";
    const std::string cmd_new_shadow =
      std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --file \""
      + input.string() + "\" 2>/dev/null";

    const CommandResult legacy_shadow = run_stdout_command(cmd_legacy_shadow);
    const CommandResult new_shadow = run_stdout_command(cmd_new_shadow);
    EXPECT_EQ(legacy_shadow.exit_code, 0) << case_name;
    EXPECT_EQ(new_shadow.exit_code, 0) << case_name;
  }
}

TEST(StyioParserEngine, ShadowCompareWritesArtifactRecordWhenDirConfigured) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions" / "t01_int_arith.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path artifact_dir = fs::temp_directory_path() / ("styio-shadow-artifacts-" + std::to_string(uniq));
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() == ".jsonl") {
      jsonl_files.push_back(entry.path());
    }
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("\"primary_engine\":\"nightly\""), std::string::npos);
  EXPECT_NE(line.find("\"shadow_engine\":\"legacy\""), std::string::npos);

  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDetailShowsZeroFallbackForMixedRouteProgram) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-shadow-mixed-route-" + std::to_string(uniq) + ".styio");
  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-route-artifacts-" + std::to_string(uniq));

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "x = 0\n";
    out << "[...] >> ?(x < 3) => {\n";
    out << "  x += 1\n";
    out << "}\n";
    out << ">_(x)\n";
  }
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    jsonl_files.push_back(entry.path());
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("\"primary_engine\":\"nightly\""), std::string::npos);
  EXPECT_NE(line.find("primary_route=nightly_subset_statements=3,legacy_fallback_statements=0"), std::string::npos);
  EXPECT_NE(line.find("nightly_internal_legacy_bridges=0"), std::string::npos);

  fs::remove(input);
  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDetailShowsZeroFallbackForResourcePostfixSubset) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path output =
    fs::temp_directory_path() / ("styio-shadow-resource-postfix-out-" + std::to_string(uniq) + ".txt");
  const fs::path input =
    fs::temp_directory_path() / ("styio-shadow-resource-postfix-" + std::to_string(uniq) + ".styio");
  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-resource-artifacts-" + std::to_string(uniq));

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "\"shadow resource postfix\" >> @file(\"" << output.string() << "\")\n";
  }
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    jsonl_files.push_back(entry.path());
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("primary_route=nightly_subset_statements=1,legacy_fallback_statements=0"), std::string::npos);

  fs::remove(output);
  fs::remove(input);
  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDetailShowsZeroFallbackForRedirectFeature) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "file_resources" / "t07_redirect.styio";
  ASSERT_TRUE(fs::exists(input));

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-redirect-artifacts-" + std::to_string(uniq));
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    jsonl_files.push_back(entry.path());
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("\"primary_engine\":\"nightly\""), std::string::npos);
  EXPECT_NE(line.find("legacy_fallback_statements=0"), std::string::npos);
  EXPECT_NE(line.find("nightly_internal_legacy_bridges=0"), std::string::npos);

  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDetailShowsZeroFallbackForIteratorSubset) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-shadow-iterator-subset-" + std::to_string(uniq) + ".styio");
  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-iterator-artifacts-" + std::to_string(uniq));

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "f <- @file(\"tests/features/file_resources/data/hello.txt\")\n";
    out << "f >> #(line) => {\n";
    out << "  >_(line)\n";
    out << "}\n";
  }
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    jsonl_files.push_back(entry.path());
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("primary_route=nightly_subset_statements=2,legacy_fallback_statements=0"), std::string::npos);

  fs::remove(input);
  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDetailShowsZeroFallbackForSnapshotDeclSubset) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-shadow-snapshot-subset-" + std::to_string(uniq) + ".styio");
  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-snapshot-artifacts-" + std::to_string(uniq));

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "ref_val = (<< @file(\"tests/features/stream_processing/data/ref.txt\"))\n";
  }
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    jsonl_files.push_back(entry.path());
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("primary_route=nightly_subset_statements=1,legacy_fallback_statements=0"), std::string::npos);
  EXPECT_NE(line.find("nightly_internal_legacy_bridges=0"), std::string::npos);

  fs::remove(input);
  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDetailTracksZeroInternalLegacyBridgesForMatchCasesSubset) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-shadow-matchcases-bridge-" + std::to_string(uniq) + ".styio");
  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-matchcases-bridge-artifacts-" + std::to_string(uniq));

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "x = 4\n";
    out << "label = x % 2 ?= {\n";
    out << "  0 => {\n";
    out << "    <| \"even\"\n";
    out << "  }\n";
    out << "  _ => {\n";
    out << "    <| \"odd\"\n";
    out << "  }\n";
    out << "}\n";
    out << ">_(label)\n";
  }
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    jsonl_files.push_back(entry.path());
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("primary_route=nightly_subset_statements=3,legacy_fallback_statements=0"), std::string::npos);
  EXPECT_NE(line.find("nightly_internal_legacy_bridges=0"), std::string::npos);

  fs::remove(input);
  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDetailShowsZeroFallbackForListIteratorSubset) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-shadow-list-iterator-subset-" + std::to_string(uniq) + ".styio");
  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-list-iterator-artifacts-" + std::to_string(uniq));

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "[1, 2, 3] >> #(n) & [4, 5, 6] >> #(m) => {\n";
    out << "  >_(n + m)\n";
    out << "}\n";
  }
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    jsonl_files.push_back(entry.path());
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("primary_route=nightly_subset_statements=1,legacy_fallback_statements=0"), std::string::npos);
  EXPECT_NE(line.find("nightly_internal_legacy_bridges=0"), std::string::npos);

  fs::remove(input);
  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDetailShowsZeroFallbackAcrossListBoundaryAfterBind) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-shadow-list-boundary-bind-" + std::to_string(uniq) + ".styio");
  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-list-boundary-bind-artifacts-" + std::to_string(uniq));

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "result = true\n";
    out << "[1, 2, 3] >> #(x) => {\n";
    out << "  result = result && (x > 0)\n";
    out << "}\n";
    out << ">_(result)\n";
  }
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    jsonl_files.push_back(entry.path());
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("primary_route=nightly_subset_statements=3,legacy_fallback_statements=0"), std::string::npos);

  fs::remove(input);
  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDetailShowsZeroFallbackForAtResourceSubset) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-shadow-at-resource-subset-" + std::to_string(uniq) + ".styio");
  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-at-resource-artifacts-" + std::to_string(uniq));

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "@file(\"tests/features/stream_processing/data/input.txt\") >> #(x) => {\n";
    out << "  >_(x)\n";
    out << "}\n";
  }
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    jsonl_files.push_back(entry.path());
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("primary_route=nightly_subset_statements=1,legacy_fallback_statements=0"), std::string::npos);

  fs::remove(input);
  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDetailShowsZeroFallbackForArbitrageFeature) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "stream_processing" / "t08_arbitrage.styio";
  ASSERT_TRUE(fs::exists(input));

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path artifact_dir =
    fs::temp_directory_path() / ("styio-shadow-arbitrage-artifacts-" + std::to_string(uniq));
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --parser-shadow-compare --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  ASSERT_EQ(result.exit_code, 0) << result.stdout_text;

  std::vector<fs::path> jsonl_files;
  for (const auto& entry : fs::directory_iterator(artifact_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    jsonl_files.push_back(entry.path());
  }
  ASSERT_EQ(jsonl_files.size(), 1U);

  std::ifstream in(jsonl_files.front());
  ASSERT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("\"status\":\"match\""), std::string::npos);
  EXPECT_NE(line.find("\"primary_engine\":\"nightly\""), std::string::npos);
  EXPECT_NE(line.find("legacy_fallback_statements=0"), std::string::npos);
  EXPECT_NE(line.find("nightly_internal_legacy_bridges=0"), std::string::npos);

  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, ShadowArtifactDirRequiresShadowCompareFlag) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "features" / "scalar_expressions" / "t01_int_arith.styio";
  ASSERT_TRUE(fs::exists(input));

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path artifact_dir = fs::temp_directory_path() / ("styio-shadow-artifacts-" + std::to_string(uniq));
  ASSERT_TRUE(fs::create_directories(artifact_dir));

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=legacy --parser-shadow-artifact-dir \""
    + artifact_dir.string() + "\" --file \"" + input.string() + "\" 2>&1";
  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 6);
  EXPECT_NE(
    result.stdout_text.find("--parser-shadow-artifact-dir requires --parser-shadow-compare"),
    std::string::npos
  );

  fs::remove_all(artifact_dir);
}

TEST(StyioParserEngine, DotChainStillRejectedConsistentlyAcrossEngines) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input = fs::temp_directory_path() / ("styio-dot-chain-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "foo.bar(1).baz(2)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd_legacy =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=legacy --file \""
    + input.string() + "\" 2>&1";
  const std::string cmd_new =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 3);
  EXPECT_EQ(newer.exit_code, 3);
  EXPECT_NE(legacy.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);
  EXPECT_NE(newer.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);

  fs::remove(input);
}

TEST(StyioParserEngine, RangeLiteralIteratesInclusivelyAndIgnoresDotCount) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input = fs::temp_directory_path() / ("styio-range-literal-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "[0..3] >> #(x) => {\n"
        << "    >_(x)\n"
        << "}\n"
        << "[1......3] >> #(y) => {\n"
        << "    >_(y)\n"
        << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, std::string("0\n1\n2\n3\n1\n2\n3\n"));

  fs::remove(input);
}

TEST(StyioParserEngine, RangeLiteralExpressionBoundsMaterializeList) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input = fs::temp_directory_path() / ("styio-range-expr-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "start = 2\n"
        << "stop = 6\n"
        << "step = 2\n"
        << "xs = [start..stop..step]\n"
        << ">_(xs)\n"
        << "down = [stop..start..-step]\n"
        << ">_(down)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, std::string("[2,4,6]\n[6,4,2]\n"));

  fs::remove(input);
}

TEST(StyioParserEngine, RangeLiteralRejectsNonIntegerExpressionBounds) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input = fs::temp_directory_path() / ("styio-range-float-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "start = 1.5\n"
        << "xs = [start..3]\n"
        << ">_(xs)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 4) << result.stdout_text;
  EXPECT_NE(
    result.stdout_text.find("range literal bounds and step must be integer expressions"),
    std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, RuntimeHelperErrorEmitsJsonlRuntimeDiagnostic) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-runtime-jsonl-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "f <- @file(\"/tmp/styio_missing_runtime_diag_"
        << uniq
        << ".txt\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5);
  EXPECT_NE(result.stdout_text.find("\"category\":\"RuntimeError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"phase\":\"runtime\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_OPEN_READ\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"subcode\":\"STYIO_RUNTIME_FILE_OPEN_READ\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"notes\":[]"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("cannot open file for read"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, RuntimeFileAcquireFailureStopsBeforeNextStatement) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-runtime-acquire-stop-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "f <- @file(\"/tmp/styio_missing_runtime_stop_"
        << uniq
        << ".txt\")\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_OPEN_READ\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, RuntimeFileIteratorOpenFailureStopsBeforeNextStatement) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-runtime-file-iter-open-stop-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "@file(\"/tmp/styio_missing_iter_runtime_stop_"
        << uniq
        << ".txt\") >> #(line) => { >_(line) }\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_OPEN_READ\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, RuntimeWriteHelperErrorEmitsJsonlRuntimeDiagnostic) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-runtime-write-jsonl-" + std::to_string(uniq) + ".styio");
  const fs::path missing_target =
    fs::temp_directory_path() / ("styio-runtime-write-missing-dir-" + std::to_string(uniq)) / "out.txt";

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "\"x\" >> @file(\"" << missing_target.string() << "\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5);
  EXPECT_NE(result.stdout_text.find("\"category\":\"RuntimeError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"phase\":\"runtime\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_OPEN_WRITE\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"subcode\":\"STYIO_RUNTIME_FILE_OPEN_WRITE\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("cannot open file for write"), std::string::npos);
  EXPECT_FALSE(fs::exists(missing_target));

  fs::remove(input);
}

TEST(StyioDiagnostics, RuntimeFileWriteFailureStopsBeforeNextStatement) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-runtime-write-stop-" + std::to_string(uniq) + ".styio");
  const fs::path missing_target =
    fs::temp_directory_path() / ("styio-runtime-write-stop-missing-dir-" + std::to_string(uniq)) / "out.txt";

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "\"x\" >> @file(\"" << missing_target.string() << "\")\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_OPEN_WRITE\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);
  EXPECT_FALSE(fs::exists(missing_target));

  fs::remove(input);
}

TEST(StyioResourceLifecycle, FlexFileRebindAfterCloseReopensSamePath) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path source =
    fs::temp_directory_path() / ("styio-resource-rebind-" + std::to_string(uniq) + ".styio");
  const fs::path data =
    fs::temp_directory_path() / ("styio-resource-rebind-data-" + std::to_string(uniq) + ".txt");

  {
    std::ofstream out(data);
    ASSERT_TRUE(out.is_open());
    out << "alpha\nbeta\n";
  }
  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << "f = @file(\"" << data.generic_string() << "\")\n";
    out << "f.close()\n";
    out << "f = @file(\"" << data.generic_string() << "\")\n";
    out << "f >> #(line) => { >_(line) }\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + source.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "alpha\nbeta\n");

  fs::remove(source);
  fs::remove(data);
}

TEST(StyioResourceLifecycle, FileAliasUseAfterCloseFailsFast) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path source =
    fs::temp_directory_path() / ("styio-resource-alias-close-" + std::to_string(uniq) + ".styio");
  const fs::path data =
    fs::temp_directory_path() / ("styio-resource-alias-close-data-" + std::to_string(uniq) + ".txt");

  {
    std::ofstream out(data);
    ASSERT_TRUE(out.is_open());
    out << "alpha\n";
  }
  {
    std::ofstream out(source);
    ASSERT_TRUE(out.is_open());
    out << "f1 <- @file(\"" << data.generic_string() << "\")\n";
    out << "f2 <- @file(\"" << data.generic_string() << "\")\n";
    out << "f1.close()\n";
    out << "f2 >> #(line) => { >_(line) }\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + source.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_INVALID_FILE_HANDLE\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("alpha"), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(source);
  fs::remove(data);
}

TEST(StyioDiagnostics, InvalidNumericStdinArgumentEmitsJsonlRuntimeDiagnostic) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-runtime-numeric-parse-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# add1 := (x: i64) => x + 1\n";
    out << "@stdin >> #(line) => {\n";
    out << "  >_(add1(line))\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("printf 'abc\\n' | \"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5);
  EXPECT_NE(result.stdout_text.find("\"category\":\"RuntimeError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"phase\":\"runtime\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_NUMERIC_PARSE\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"subcode\":\"STYIO_RUNTIME_NUMERIC_PARSE\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("cannot parse integer from string"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, CompoundAssignOnImmutableBindingReportsTypeError) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-compound-immutable-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "x : i64 := 1\n";
    out << "x += 2\n";
    out << ">_(x)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 4);
  EXPECT_NE(result.stdout_text.find("\"category\":\"TypeError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"phase\":\"type\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_TYPE_ERROR\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"notes\":[]"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("compound assignment requires a mutable binding"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, StreamZipUnsupportedSourceReportsTypeError) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-zip-unsupported-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "x = 1\n";
    out << "x >> #(n) & @file(\"tests/features/stream_processing/data/ref.txt\") >> #(s) => {\n";
    out << "  >_(s + n)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 4);
  EXPECT_NE(result.stdout_text.find("\"category\":\"TypeError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_TYPE_ERROR\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("zip requires iterable inputs on both sides"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, IteratorSequenceHashTagRoutingFailsClosed) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-iterseq-hashtag-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "[1, 2] >> #price\n";
    out << ">_(99)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 4);
  EXPECT_NE(result.stdout_text.find("\"category\":\"TypeError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_TYPE_ERROR\""), std::string::npos);
  EXPECT_NE(
    result.stdout_text.find("iterator sequence hash-tag routing is not implemented"),
    std::string::npos
  );

  fs::remove(input);
}

TEST(StyioDiagnostics, RetiredLegacyStateDeclReportsParseError) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-series-window-non-literal-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "[1, 2, 3] >> #(p) => {\n";
    out << "  @[3](ma = p)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 3);
  EXPECT_NE(result.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("legacy state-resource @[...] syntax is retired"), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("Styio.NotImplemented"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, SingleArgStateFunctionInliningUsesCallArgument) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-state-inline-arg-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "@out : i64|..3|\n";
    out << "[1, 2, 3] >> #(v) => {\n";
    out << "  next = @out[-1] + v\n";
    out << "  next -> @out\n";
    out << "  >_(next)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "1\n3\n6\n");
  EXPECT_EQ(result.stdout_text.find("unsupported AST node in inlined state expression clone"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, BlockStateFunctionInliningUsesCallArgument) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-state-inline-block-arg-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "@out : i64|..3|\n";
    out << "[1, 2, 3] >> #(v) => {\n";
    out << "  next = @out[-1] + v\n";
    out << "  next -> @out\n";
    out << "  >_(next)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "1\n3\n6\n");
  EXPECT_EQ(result.stdout_text.find("unsupported AST node in inlined state expression clone"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, StateInlineMatchCasesFunctionUsesCallArgument) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-state-inline-match-arg-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "@out : i64|..3|\n";
    out << "[1, 2, 3] >> #(v) => {\n";
    out << "  next = v ?= {\n";
    out << "    1 => { <| @out[-1] + 10 }\n";
    out << "    _ => { <| @out[-1] + v }\n";
    out << "  }\n";
    out << "  next -> @out\n";
    out << "  >_(next)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "10\n12\n15\n");
  EXPECT_EQ(result.stdout_text.find("unsupported AST node in inlined state expression clone"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, StateInlineInfiniteLiteralFunctionUsesCallArgument) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-state-inline-infinite-arg-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "@out : i64|..2|\n";
    out << "[1, 2] >> #(v) => {\n";
    out << "  0 -> @out\n";
    out << "  >_(0)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "0\n0\n");
  EXPECT_EQ(result.stdout_text.find("unsupported AST node in inlined state expression clone"), std::string::npos);

  fs::remove(input);
}

TEST(StyioTopologyV2, LogicalResourceWritesCommitAtPulseEnd) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-topology-v2-lazy-commit-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "@x : i64|2|\n";
    out << "[1, 2] >> #(v) => {\n";
    out << "  v -> @x\n";
    out << "  >_(@x[-1])\n";
    out << "}\n";
    out << ">_(@x[-2])\n";
    out << ">_(@x[-1])\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "0\n1\n1\n2\n");

  fs::remove(input);
}

TEST(StyioTopologyV2, AwaitPipeResourceEffectDiscardCommitsWrite) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-discard-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "@out : i64|..2|\n";
    out << "[1] >> #(v) => {\n";
    out << "  ?| v -> @out | ...\n";
    out << "  >_(@out[-1])\n";
    out << "}\n";
    out << ">_(@out[-1])\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "0\n1\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, FallbackRunsAfterFileWriteFailureAndContinues) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-fallback-" + std::to_string(uniq) + ".styio");
  const fs::path missing_target =
    fs::temp_directory_path() / ("styio-resource-effect-missing-dir-" + std::to_string(uniq)) / "out.txt";

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| \"primary\" -> @file(\"" << missing_target.string()
        << "\") | \"fallback\" -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "fallback\nafter\n");
  EXPECT_FALSE(fs::exists(missing_target));

  fs::remove(input);
}

TEST(StyioResourceEffects, FallbackIsSkippedAfterSuccessfulFileWrite) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-success-" + std::to_string(uniq) + ".styio");
  const fs::path output =
    fs::temp_directory_path() / ("styio-resource-effect-success-" + std::to_string(uniq) + ".txt");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| \"primary\" -> @file(\"" << output.string()
        << "\") | \"fallback\" -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "after\n");
  ASSERT_TRUE(fs::exists(output));
  EXPECT_EQ(read_text_file_latest(output), "primary");

  fs::remove(input);
  fs::remove(output);
}

TEST(StyioResourceEffects, ResourceMethodCloseSuccessSkipsFallback) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-close-success-" + std::to_string(uniq) + ".styio");
  const fs::path data =
    fs::temp_directory_path() / ("styio-resource-effect-close-success-" + std::to_string(uniq) + ".txt");

  {
    std::ofstream out(data);
    ASSERT_TRUE(out.is_open());
    out << "alpha\n";
  }
  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| @file(\"" << data.generic_string()
        << "\").close() | \"fallback\" -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "after\n");

  fs::remove(input);
  fs::remove(data);
}

TEST(StyioResourceEffects, ResourceMethodCloseFallbackRecoversFileOpenFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-close-fallback-" + std::to_string(uniq) + ".styio");
  const fs::path missing =
    fs::temp_directory_path() / ("styio-resource-effect-close-missing-" + std::to_string(uniq) + ".txt");

  fs::remove(missing);
  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| @file(\"" << missing.generic_string()
        << "\").close() | \"fallback\" -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "fallback\nafter\n");
  EXPECT_FALSE(fs::exists(missing));

  fs::remove(input);
}

TEST(StyioResourceEffects, ResourceMethodCloseNamedIoHandlerRecoversFileOpenFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-close-handler-" + std::to_string(uniq) + ".styio");
  const fs::path missing =
    fs::temp_directory_path() / ("styio-resource-effect-close-handler-missing-" + std::to_string(uniq) + ".txt");

  fs::remove(missing);
  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| @file(\"" << missing.generic_string()
        << "\").close() | parse => \"parse\" -> @stdout | io => \"io\" -> @stdout | \"fallback\" -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "io\nafter\n");
  EXPECT_EQ(result.stdout_text.find("parse"), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("fallback"), std::string::npos);
  EXPECT_FALSE(fs::exists(missing));

  fs::remove(input);
}

TEST(StyioResourceEffects, ResourceMethodCloseNoFallbackStopsFileOpenFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-close-no-fallback-" + std::to_string(uniq) + ".styio");
  const fs::path missing =
    fs::temp_directory_path() / ("styio-resource-effect-close-no-fallback-missing-" + std::to_string(uniq) + ".txt");

  fs::remove(missing);
  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| @file(\"" << missing.generic_string() << "\").close()\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_OPEN_READ\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);
  EXPECT_FALSE(fs::exists(missing));

  fs::remove(input);
}

TEST(StyioResourceEffects, NamedIoHandlerRunsForFileWriteFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-io-handler-" + std::to_string(uniq) + ".styio");
  const fs::path missing_target =
    fs::temp_directory_path() / ("styio-resource-effect-io-handler-dir-" + std::to_string(uniq)) / "out.txt";

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| \"primary\" -> @file(\"" << missing_target.string()
        << "\") | parse => \"parse\" -> @stdout | io => \"io\" -> @stdout | \"fallback\" -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "io\nafter\n");
  EXPECT_FALSE(fs::exists(missing_target));

  fs::remove(input);
}

TEST(StyioResourceEffects, NamedCleanupHandlerRunsForFileCloseFailure) {
#ifdef _WIN32
  GTEST_SKIP() << "/dev/full cleanup-failure fixture is Unix-specific";
#else
  if (!fs::exists("/dev/full")) {
    GTEST_SKIP() << "/dev/full is not available";
  }

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-cleanup-handler-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| \"primary\" -> @file(\"/dev/full\")"
        << " | cleanup => \"cleanup\" -> @stdout"
        << " | io => \"io\" -> @stdout"
        << " | \"fallback\" -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "cleanup\nafter\n");
  EXPECT_EQ(result.stdout_text.find("io"), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("fallback"), std::string::npos);

  fs::remove(input);
#endif
}

TEST(StyioResourceEffects, IoHandlerDoesNotCatchFileCloseCleanupFailure) {
#ifdef _WIN32
  GTEST_SKIP() << "/dev/full cleanup-failure fixture is Unix-specific";
#else
  if (!fs::exists("/dev/full")) {
    GTEST_SKIP() << "/dev/full is not available";
  }

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-cleanup-unmatched-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| \"primary\" -> @file(\"/dev/full\") | io => \"io\" -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_CLEANUP_FAILURE\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("\nio\n"), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
#endif
}

TEST(StyioResourceEffects, UnmatchedNamedHandlerFailsFastWithoutCatchAllFallback) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-unmatched-handler-" + std::to_string(uniq) + ".styio");
  const fs::path missing_target =
    fs::temp_directory_path() / ("styio-resource-effect-unmatched-handler-dir-" + std::to_string(uniq)) / "out.txt";

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| \"primary\" -> @file(\"" << missing_target.string()
        << "\") | backpressure => \"pressure\" -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_OPEN_WRITE\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("pressure"), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);
  EXPECT_FALSE(fs::exists(missing_target));

  fs::remove(input);
}

TEST(StyioResourceEffects, HandlerChainFallsBackAfterUnmatchedEffects) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-handler-chain-" + std::to_string(uniq) + ".styio");
  const fs::path missing_target =
    fs::temp_directory_path() / ("styio-resource-effect-handler-chain-dir-" + std::to_string(uniq)) / "out.txt";

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| \"primary\" -> @file(\"" << missing_target.string()
        << "\") | parse => \"parse\" -> @stdout | \"fallback\" -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "fallback\nafter\n");
  EXPECT_FALSE(fs::exists(missing_target));

  fs::remove(input);
}

TEST(StyioResourceEffects, MissingFallbackStopsAtSettlementSite) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-no-fallback-" + std::to_string(uniq) + ".styio");
  const fs::path missing_target =
    fs::temp_directory_path() / ("styio-resource-effect-no-fallback-dir-" + std::to_string(uniq)) / "out.txt";

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "?| \"primary\" -> @file(\"" << missing_target.string() << "\")\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_OPEN_WRITE\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);
  EXPECT_FALSE(fs::exists(missing_target));

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueFallbackRecoversFileInstantPull) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-value-fallback-" + std::to_string(uniq) + ".styio");
  const fs::path data =
    fs::temp_directory_path() / ("styio-resource-effect-value-data-" + std::to_string(uniq) + ".txt");
  const fs::path missing =
    fs::temp_directory_path() / ("styio-resource-effect-value-missing-" + std::to_string(uniq) + ".txt");

  {
    std::ofstream data_out(data);
    ASSERT_TRUE(data_out.is_open());
    data_out << "42\n";
  }

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "success = ?| (<< @file(\"" << data.string() << "\")) | 7\n";
    out << ">_(success)\n";
    out << "recovered = ?| (<< @file(\"" << missing.string() << "\")) | 7\n";
    out << ">_(recovered)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "42\n7\nafter\n");

  fs::remove(input);
  fs::remove(data);
}

TEST(StyioResourceEffects, ValueNamedHandlerRecoversFileInstantPull) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-value-handler-" + std::to_string(uniq) + ".styio");
  const fs::path missing =
    fs::temp_directory_path() / ("styio-resource-effect-value-handler-missing-" + std::to_string(uniq) + ".txt");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "handled = ?| (<< @file(\"" << missing.string()
        << "\")) | parse => 8 | io => 9 | 7\n";
    out << ">_(handled)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "9\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueNoFallbackStopsFileInstantPull) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-value-no-fallback-" + std::to_string(uniq) + ".styio");
  const fs::path missing =
    fs::temp_directory_path() / ("styio-resource-effect-value-no-fallback-missing-" + std::to_string(uniq) + ".txt");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "result = ?| (<< @file(\"" << missing.string() << "\"))\n";
    out << ">_(result)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_OPEN_READ\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueListIndexSuccessSkipsFallback) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-list-index-success-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "xs = [10,20]\n";
    out << "value = ?| xs[1] | 99\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "20\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueListIndexFallbackRecoversBoundsFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-list-index-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "xs = [10,20]\n";
    out << "value = ?| xs[5] | 99\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "99\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueListIndexNamedBoundsHandlerRecoversBoundsFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-list-index-handler-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "xs = [10,20]\n";
    out << "value = ?| xs[5] | io => 1 | bounds => 77 | 99\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "77\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueListIndexNoFallbackStopsBoundsFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-list-index-no-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "xs = [10,20]\n";
    out << "value = ?| xs[5]\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_LIST_INDEX\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, RuntimeListIndexFailureStopsBeforeNextStatement) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-list-index-default-guard-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "xs = [1,2]\n";
    out << ">_(xs[9])\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_LIST_INDEX\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueDictIndexSuccessSkipsFallback) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-dict-index-success-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "d = dict{\"a\": 10, \"b\": 20}\n";
    out << "value = ?| d[\"b\"] | 99\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "20\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueDictIndexFallbackRecoversBoundsFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-dict-index-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "d = dict{\"a\": 10, \"b\": 20}\n";
    out << "value = ?| d[\"missing\"] | 99\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "99\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueDictIndexNamedBoundsHandlerRecoversBoundsFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-dict-index-handler-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "d = dict{\"a\": 10, \"b\": 20}\n";
    out << "value = ?| d[\"missing\"] | io => 1 | bounds => 77 | 99\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "77\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueDictIndexNoFallbackStopsBoundsFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-dict-index-no-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "d = dict{\"a\": 10, \"b\": 20}\n";
    out << "value = ?| d[\"missing\"]\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_DICT_KEY\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, RuntimeDictKeyFailureStopsBeforeNextStatement) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-dict-key-default-guard-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "d = dict{\"a\": 1}\n";
    out << ">_(d[\"missing\"])\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_DICT_KEY\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueMatrixIndexSuccessSkipsFallback) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-matrix-index-success-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "m: matrix = [[1,2],[3,4]]\n";
    out << "value = ?| m[1][1] | 99\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "4\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueMatrixIndexFallbackRecoversBoundsFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-matrix-index-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "m: matrix = [[1,2],[3,4]]\n";
    out << "value = ?| m[3][0] | 99\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "99\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueMatrixIndexNamedBoundsHandlerRecoversBoundsFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-matrix-index-handler-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "m: matrix = [[1,2],[3,4]]\n";
    out << "value = ?| m[3][0] | io => 1 | bounds => 77 | 99\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "77\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueMatrixIndexNoFallbackStopsBoundsFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-matrix-index-no-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "m: matrix = [[1,2],[3,4]]\n";
    out << "value = ?| m[3][0]\n";
    out << ">_(value)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_MATRIX_INDEX\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueMatrixRowFallbackRecoversBoundsFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-matrix-row-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "m: matrix = [[1,2],[3,4]]\n";
    out << "row: list[i64] = ?| m[3] | [9]\n";
    out << ">_(row)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "[9]\nafter\n");

  fs::remove(input);
}

TEST(StyioDiagnostics, RuntimeMatrixIndexFailureStopsBeforeNextStatement) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-matrix-index-default-guard-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "m: matrix = [[1,2],[3,4]]\n";
    out << ">_(m[3][0])\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_MATRIX_INDEX\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueFallbackRecoversStdinInstantPullParseFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-stdin-value-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "success = ?| (<- @stdin) | 7\n";
    out << ">_(success)\n";
    out << "recovered = ?| (<- @stdin) | 7\n";
    out << ">_(recovered)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("printf '42\\nbad\\n' | \"") + runner + "\" --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "42\n7\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueNamedHandlerRecoversStdinInstantPullParseFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-stdin-value-handler-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "handled = ?| (<- @stdin) | io => 8 | parse => 9 | 7\n";
    out << ">_(handled)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("printf 'bad\\n' | \"") + runner + "\" --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "9\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueNoFallbackStopsStdinInstantPullParseFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-stdin-value-no-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "result = ?| (<- @stdin)\n";
    out << ">_(result)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("printf 'bad\\n' | \"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_NUMERIC_PARSE\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueTypedF64FallbackAndHandlerRecoverStdinInstantPullParseFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-stdin-f64-value-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "success: f64 = ?| (<- @stdin) | 7.5\n";
    out << ">_(success)\n";
    out << "recovered: f64 = ?| (<- @stdin) | 7.5\n";
    out << ">_(recovered)\n";
    out << "handled: f64 = ?| (<- @stdin) | parse => 9.5 | 7.5\n";
    out << ">_(handled)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("printf '3.25\\nbad\\nbad\\n' | \"") + runner + "\" --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "3.250000\n7.500000\n9.500000\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueTypedStringReturnsStdinInstantPull) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-stdin-string-value-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "value: string = ?| (<- @stdin) | \"fallback\"\n";
    out << ">_(value)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("printf 'hello\\n' | \"") + runner + "\" --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "hello\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueTypedListReturnsStdinInstantPull) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-stdin-list-value-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "values: list[i64] = ?| (<- @stdin) | [7,8]\n";
    out << ">_(values)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("printf '[1,2]\\n' | \"") + runner + "\" --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "[1,2]\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueTypedListFallbackRecoversStdinInstantPullParseFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-stdin-list-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "values: list[i64] = ?| (<- @stdin) | [7,8]\n";
    out << ">_(values)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("printf 'bad-list\\n' | \"") + runner + "\" --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "[7,8]\nafter\n");

  fs::remove(input);
}

TEST(StyioResourceEffects, ValueTypedListNoFallbackStopsStdinInstantPullParseFailure) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-effect-stdin-list-no-fallback-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "values: list[i64] = ?| (<- @stdin)\n";
    out << ">_(values)\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("printf 'bad-list\\n' | \"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_LIST_PARSE\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);

  fs::remove(input);
}

TEST(StyioResourceEffects, TaskAwaitFallbackRunsAfterFailedTaskPull) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-task_await-fallback-" + std::to_string(uniq) + ".styio");
  const fs::path missing_target =
    fs::temp_directory_path() / ("styio-task_await-fallback-dir-" + std::to_string(uniq)) / "out.txt";

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "job = ||> {\n";
    out << "  ?| \"primary\" -> @file(\"" << missing_target.string() << "\")\n";
    out << "  <| 11\n";
    out << "}\n";
    out << "?| job -> answer: i64 | 7\n";
    out << "answer -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0) << result.stdout_text;
  EXPECT_EQ(result.stdout_text, "7\nafter\n");
  EXPECT_FALSE(fs::exists(missing_target));

  fs::remove(input);
}

TEST(StyioResourceEffects, FailedTaskAwaitWithoutFallbackStopsAtSettlementSite) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-task_await-no-fallback-" + std::to_string(uniq) + ".styio");
  const fs::path missing_target =
    fs::temp_directory_path() / ("styio-task_await-no-fallback-dir-" + std::to_string(uniq)) / "out.txt";

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "job = ||> {\n";
    out << "  ?| \"primary\" -> @file(\"" << missing_target.string() << "\")\n";
    out << "  <| 11\n";
    out << "}\n";
    out << "?| job -> answer: i64\n";
    out << "answer -> @stdout\n";
    out << ">_(\"after\")\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 5) << result.stdout_text;
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME_FILE_OPEN_WRITE\""), std::string::npos);
  EXPECT_EQ(result.stdout_text.find("after"), std::string::npos);
  EXPECT_FALSE(fs::exists(missing_target));

  fs::remove(input);
}

TEST(StyioDiagnostics, MatchWithoutDefaultDoesNotCrash) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-match-without-default-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "x = 1\n";
    out << "x ?= {\n";
    out << "  1 => >_(1)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "1\n");

  fs::remove(input);
}

TEST(StyioDiagnostics, FunctionMatchSugarAndTailExpressionsReturnValues) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-function-match-tail-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# fib := (n: i32) ?= {\n";
    out << "  0 => 0\n";
    out << "  1 => 1\n";
    out << "  _ => fib(n - 1) + fib(n - 2)\n";
    out << "}\n";
    out << "# block_tail := (n: i32) => {\n";
    out << "  x = n + 1\n";
    out << "  x\n";
    out << "}\n";
    out << "# branch_block_tail := (n: i32) ?= {\n";
    out << "  0 => {\n";
    out << "    base = 40\n";
    out << "    base + 2\n";
    out << "  }\n";
    out << "  _ => n + 10\n";
    out << "}\n";
    out << "# float_match_tail := (n: i32) ?= {\n";
    out << "  0 => 1.5\n";
    out << "  _ => n + 2.5\n";
    out << "}\n";
    out << "# stmt_tail := () => {\n";
    out << "  x = 7\n";
    out << "}\n";
    out << ">_(fib(6))\n";
    out << ">_(block_tail(4))\n";
    out << ">_(branch_block_tail(0))\n";
    out << ">_(branch_block_tail(5))\n";
    out << ">_(float_match_tail(0))\n";
    out << ">_(float_match_tail(3))\n";
    out << ">_(stmt_tail())\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "8\n5\n42\n15\n1.500000\n5.500000\n0\n");

  fs::remove(input);
}

TEST(StyioDiagnostics, ScalarAndInferredFunctionReturnsStayExecutable) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-function-return-positive-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# scalar : i64 := (x: i64) => x + 1\n";
    out << "# inferred := (x: i64) => {\n";
    out << "  x + 2\n";
    out << "}\n";
    out << ">_(scalar(2))\n";
    out << ">_(inferred(2))\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "3\n4\n");

  fs::remove(input);
}

TEST(StyioDiagnostics, TupleFunctionReturnAnnotationFailsClosed) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-function-tuple-return-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# pair : (i64, i64) := (x: i64) => x\n";
    out << ">_(pair(7))\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=nightly --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 4);
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_TYPE_ERROR\""), std::string::npos);
  EXPECT_NE(
    result.stdout_text.find("tuple function return annotations require tuple value IR"),
    std::string::npos);
  EXPECT_EQ(result.stdout_text.find("\n7\n"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, MalformedStatementPrefixReportsParseErrorWithoutCrash) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-malformed-stmt-prefix-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << " ?* {>_(1 =xx";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --error-format=jsonl --file \""
    + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 3);
  EXPECT_NE(result.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);
  EXPECT_NE(
    result.stdout_text.find("unsupported syntax in authoritative nightly parser"),
    std::string::npos);
  EXPECT_EQ(result.stdout_text.find("Styio.NotImplemented"), std::string::npos);

  fs::remove(input);
}

TEST(StyioSamples, BubbleSortListInput) {
  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const fs::path sample_path =
    fs::path(STYIO_SOURCE_DIR) / "example" / "algorithms" / "bubble_sort.styio";
  ASSERT_TRUE(fs::exists(sample_path));

  const std::string cmd =
    std::string("printf '[5,1,4,2,8]' | \"") + runner + "\" --file \"" + sample_path.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "[1,2,4,5,8]\n");
}

TEST(StyioSamples, DictTypeBasics) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-dict-type-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "d = dict{\"a\": 1, \"b\": 2}\n";
    out << ">_(d[\"a\"])\n";
    out << ">_(d.length)\n";
    out << ">_(d.keys)\n";
    out << ">_(d.values)\n";
    out << "d[\"c\"] = 3\n";
    out << ">_(d)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "1\n2\n[\"a\",\"b\"]\n[1,2]\n{\"a\":1,\"b\":2,\"c\":3}\n");

  fs::remove(input);
}

TEST(StyioSamples, DictTypeBasicsLinearImpl) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-dict-type-linear-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "d = dict{\"a\": 1, \"b\": 2}\n";
    out << ">_(d[\"a\"])\n";
    out << ">_(d.length)\n";
    out << "d[\"c\"] = 3\n";
    out << ">_(d)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --dict-impl=linear --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "1\n2\n{\"a\":1,\"b\":2,\"c\":3}\n");

  fs::remove(input);
}

TEST(StyioSamples, DictTypeScalarFamilies) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-dict-families-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "flags = dict{\"ok\": true, \"ng\": false}\n";
    out << ">_(flags[\"ok\"])\n";
    out << ">_(flags.values)\n";
    out << "names = dict{\"first\": \"Ada\", \"last\": \"Lovelace\"}\n";
    out << ">_(names[\"last\"])\n";
    out << ">_(names.values)\n";
    out << "nums = dict{\"pi\": 3.5, \"e\": 2}\n";
    out << ">_(nums[\"pi\"])\n";
    out << ">_(nums.values)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(
    result.stdout_text,
    "true\n[true,false]\nLovelace\n[\"Ada\",\"Lovelace\"]\n3.500000\n[3.500000,2.000000]\n"
  );

  fs::remove(input);
}

TEST(StyioSamples, DictTypeHandleFamilies) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-dict-handles-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "d = dict{\"nums\": [1,2,3], \"more\": [4,5]}\n";
    out << ">_(d[\"nums\"])\n";
    out << ">_(d.values)\n";
    out << "child = dict{\"left\": dict{\"x\": 1}, \"right\": dict{\"y\": 2}}\n";
    out << ">_(child[\"left\"])\n";
    out << ">_(child.values)\n";
    out << "vals = d.values\n";
    out << "vals >> #(xs) => {\n";
    out << "  >_(xs)\n";
    out << "}\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(
    result.stdout_text,
    "[1,2,3]\n[[1,2,3],[4,5]]\n{\"x\":1}\n[{\"x\":1},{\"y\":2}]\n[1,2,3]\n[4,5]\n"
  );

  fs::remove(input);
}

TEST(StyioResourceCopy, ExplicitListAndDictCloneAreIndependent) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-copy-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "nums = [1,2]\n";
    out << "nums_copy << nums\n";
    out << "nums.push(3)\n";
    out << "nums[0] = 9\n";
    out << ">_(nums_copy)\n";
    out << ">_(nums)\n";
    out << "bags = [[1,2]]\n";
    out << "bags_copy << bags\n";
    out << "bags[0] = [9]\n";
    out << ">_(bags_copy)\n";
    out << ">_(bags)\n";
    out << "d = dict{\"nums\": [1,2], \"more\": [4]}\n";
    out << "d_copy << d\n";
    out << "d[\"nums\"] = [9]\n";
    out << "d[\"extra\"] = [5]\n";
    out << ">_(d_copy)\n";
    out << ">_(d)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(
    result.stdout_text,
    "[1,2]\n[9,2,3]\n[[1,2]]\n[[9]]\n"
    "{\"nums\":[1,2],\"more\":[4]}\n{\"nums\":[9],\"more\":[4],\"extra\":[5]}\n"
  );

  fs::remove(input);
}

TEST(StyioResourceCopy, ExplicitMatrixCloneIsIndependent) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-resource-copy-matrix-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "m: matrix = [[1,2],[3,4]]\n";
    out << "m_copy << m\n";
    out << "ok = mat_set(m,0,0,9)\n";
    out << ">_(m_copy)\n";
    out << ">_(m)\n";
    out << "mf: matrix = [[1.5,2.5],[3.5,4.5]]\n";
    out << "mf_copy << mf\n";
    out << "ok2 = mat_set(mf,1,1,9.5)\n";
    out << ">_(mf_copy)\n";
    out << ">_(mf)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(
    result.stdout_text,
    "[[1,2],[3,4]]\n[[9,2],[3,4]]\n"
    "[[1.500000,2.500000],[3.500000,4.500000]]\n"
    "[[1.500000,2.500000],[3.500000,9.500000]]\n"
  );

  fs::remove(input);
}

TEST(StyioSamples, MatrixTypeNestedListLiteral) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-matrix-type-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "m: matrix = [[1,0],[0,1]]\n";
    out << ">_(m)\n";
    out << ">_(m[0])\n";
    out << ">_(m[1][1])\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "[[1,0],[0,1]]\n[1,0]\n1\n");

  fs::remove(input);
}

TEST(StyioSamples, MatrixOperationsAndIntrinsics) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-matrix-ops-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "a: matrix = [[1,2],[3,4]]\n";
    out << "b: matrix = [[5,6],[7,8]]\n";
    out << ">_(a + b)\n";
    out << ">_(a * b)\n";
    out << ">_(mat_hadamard(a,b))\n";
    out << ">_(transpose(a))\n";
    out << ">_(mat_identity_i64(2))\n";
    out << "z = mat_zeros_i64(2,2)\n";
    out << "ok = mat_set(z,0,1,9)\n";
    out << ">_(z)\n";
    out << ">_(dot(a,b))\n";
    out << ">_(mat_sum(a))\n";
    out << ">_(norm(a))\n";
    out << ">_(mat_shape(a))\n";
    out << ">_(mat_rows(a))\n";
    out << ">_(mat_cols(a))\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --parser-engine=nightly --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(
    result.stdout_text,
    "[[6,8],[10,12]]\n"
    "[[19,22],[43,50]]\n"
    "[[5,12],[21,32]]\n"
    "[[1,3],[2,4]]\n"
    "[[1,0],[0,1]]\n"
    "[[0,9],[0,0]]\n"
    "70\n"
    "10\n"
    "5.477226\n"
    "[2,2]\n"
    "2\n"
    "2\n"
  );

  fs::remove(input);
}

TEST(StyioSamples, ListPredefinedOperations) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-list-ops-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "nums = [1,2]\n";
    out << "nums.push(3)\n";
    out << "nums.insert(0,4)\n";
    out << "nums.pop()\n";
    out << ">_(nums)\n";
    out << "flags = [true,false]\n";
    out << "flags[1] = true\n";
    out << ">_(flags)\n";
    out << "names = [\"Ada\"]\n";
    out << "names.push(\"Lovelace\")\n";
    out << "names.insert(1, \"Byron\")\n";
    out << "names.pop()\n";
    out << ">_(names)\n";
    out << "bags = [[1,2]]\n";
    out << "bags.push([3])\n";
    out << "bags[0] = [9]\n";
    out << ">_(bags)\n";
    out << "maps = [dict{\"a\": 1}]\n";
    out << "maps.insert(1, dict{\"b\": 2})\n";
    out << "maps[0] = dict{\"c\": 3}\n";
    out << ">_(maps)\n";
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');

  const std::string cmd =
    std::string("\"") + runner + "\" --file \"" + input.string() + "\" 2>&1";

  const CommandResult result = run_stdout_command(cmd);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(
    result.stdout_text,
    "[4,1,2]\n[true,true]\n[\"Ada\",\"Byron\"]\n[[9],[3]]\n[{\"c\":3},{\"b\":2}]\n"
  );

  fs::remove(input);
}
