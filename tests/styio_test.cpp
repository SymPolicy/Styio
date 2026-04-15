#include <gtest/gtest.h>

#include <cstdlib>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#include "StyioTesting/PipelineCheck.hpp"

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

namespace {

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

} // namespace

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
  const fs::path factor_path("/tmp/styio_pipeline_factor.txt");
  const fs::path expected_factor =
    case_dir / "expected" / "factor_file.txt";
  {
    std::ofstream out(factor_path, std::ios::binary | std::ios::trunc);
    out << read_text_file_latest(expected_factor);
  }

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }
  const std::string err = styio::testing::run_pipeline_case(case_dir.string(), runner);
  EXPECT_EQ(err, "") << err;
  fs::remove(factor_path);
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
  EXPECT_NE(result.stdout_text.find("\"channel\":\"stable\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"supported_contracts\":{\"compile_plan\":[]}"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"machine_info_json\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"single_file_entry\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"jsonl_diagnostics\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"nano_package_registry_publish_v1\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"dict_impl\":{\"selected\":\"ordered-hash\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"edition_max\":\"2026\""), std::string::npos);
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
    std::string::npos);

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
      ec);
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
      ec);
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM1Sample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1" / "t01_int_arith.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM1TypedBindSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1" / "t07_typed_bind.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM1CompoundAssignSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1" / "t14_compound.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM2SimpleFuncSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m2" / "t01_simple_func.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM3MatchExprSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m3" / "t02_match_expr.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM4WaveMergeSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m4" / "t01_wave_merge.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM4WaveDispatchSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m4" / "t03_wave_dispatch.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM5WriteFileSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m5" / "t02_write_file.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM5RedirectSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m5" / "t07_redirect.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM5ReadFileSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m5" / "t01_read_file.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM5AutoDetectSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m5" / "t05_auto_detect.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM5PipeFuncSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m5" / "t08_pipe_func.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM9StdoutBoolSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m9" / "t04_stdout_bool.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM7InstantPullSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m7" / "t04_instant_pull.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM7SnapshotSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m7" / "t03_snapshot.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM7ZipCollectionsSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m7" / "t01_zip_collections.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM7ZipUnequalSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m7" / "t02_zip_unequal.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM7ZipFilesSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m7" / "t05_zip_files.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM7ArbitrageSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m7" / "t08_arbitrage.styio";
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

TEST(StyioParserEngine, LegacyAndNightlyMatchOnM7FullPipelineSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m7" / "t10_full_pipeline.styio";
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
  const fs::path input = fs::temp_directory_path() / ("styio-match-pointer-scrutinee-"
                                                      + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "f <- @file{\"tests/m5/data/input.txt\"}\n";
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
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1" / "t01_int_arith.styio";
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
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1" / "t01_int_arith.styio";
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
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1" / "t01_int_arith.styio";
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

TEST(StyioParserEngine, ShadowCompareAcceptsM1TypedBindSample) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1" / "t07_typed_bind.styio";
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

TEST(StyioParserEngine, ShadowCompareAcceptsM1CoreSuite) {
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
    const fs::path input = fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1" / name;
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

TEST(StyioParserEngine, ShadowCompareAcceptsM1FullSuite) {
  const fs::path m1_dir = fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1";
  ASSERT_TRUE(fs::exists(m1_dir));

  std::vector<fs::path> inputs;
  for (const auto& entry : fs::directory_iterator(m1_dir)) {
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

TEST(StyioParserEngine, ShadowCompareAcceptsM2CoreSuite) {
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
    const fs::path input = fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m2" / name;
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

TEST(StyioParserEngine, ShadowCompareAcceptsM2FullSuite) {
  const fs::path m2_dir = fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m2";
  ASSERT_TRUE(fs::exists(m2_dir));

  std::vector<fs::path> inputs;
  for (const auto& entry : fs::directory_iterator(m2_dir)) {
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
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1" / "t01_int_arith.styio";
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
    out << "[...] ?(x < 3) >> {\n";
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
    out << "\"shadow resource postfix\" >> @file{\"" << output.string() << "\"}\n";
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
    out << "f <- @file{\"tests/m5/data/hello.txt\"}\n";
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
    out << "@[ref_val] << @file{\"tests/m7/data/ref.txt\"}\n";
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
    out << "@file{\"tests/m7/data/input.txt\"} >> #(x) => {\n";
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

TEST(StyioParserEngine, ShadowArtifactDirRequiresShadowCompareFlag) {
  const fs::path input =
    fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m1" / "t01_int_arith.styio";
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
    std::string::npos);

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

TEST(StyioDiagnostics, RuntimeHelperErrorEmitsJsonlRuntimeDiagnostic) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-runtime-jsonl-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "f <- @file{\"/tmp/styio_missing_runtime_diag_"
        << uniq
        << ".txt\"}\n";
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
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"subcode\":\"STYIO_RUNTIME_FILE_OPEN_READ\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("cannot open file for read"), std::string::npos);

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
    out << "\"x\" >> @file{\"" << missing_target.string() << "\"}\n";
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
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("\"subcode\":\"STYIO_RUNTIME_FILE_OPEN_WRITE\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("cannot open file for write"), std::string::npos);
  EXPECT_FALSE(fs::exists(missing_target));

  fs::remove(input);
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
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_RUNTIME\""), std::string::npos);
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
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_TYPE\""), std::string::npos);
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
    out << "a = [1, 2, 3]\n";
    out << "a >> #(n) & [\"x\", \"y\", \"z\"] >> #(s) => {\n";
    out << "  >_(n + \" \" + s)\n";
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
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_TYPE\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("unsupported stream zip lowering"), std::string::npos);

  fs::remove(input);
}

TEST(StyioDiagnostics, SeriesIntrinsicWindowNonLiteralReportsTypeError) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input =
    fs::temp_directory_path() / ("styio-series-window-non-literal-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "[1, 2, 3] >> #(p) => {\n";
    out << "  n = 3\n";
    out << "  @[3](ma = p[avg, n])\n";
    out << "  >_(ma)\n";
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
  EXPECT_NE(result.stdout_text.find("\"code\":\"STYIO_TYPE\""), std::string::npos);
  EXPECT_NE(result.stdout_text.find("window size for series intrinsic must be integer literal"), std::string::npos);
  EXPECT_NE(result.stdout_text.find("Styio.TypeError"), std::string::npos);
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
    out << "# pulse = (x) => @[sum = 0](out = $sum + x)\n";
    out << "[1, 2, 3] >> #(v) => {\n";
    out << "  pulse(v)\n";
    out << "  >_(out)\n";
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
    out << "# pulse = (x) => {\n";
    out << "  @[sum = 0](out = $sum + x)\n";
    out << "}\n";
    out << "[1, 2, 3] >> #(v) => {\n";
    out << "  pulse(v)\n";
    out << "  >_(out)\n";
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
    out << "# pulse = (x) => @[sum = 0](out = x ?= {\n";
    out << "  1 => { <| $sum + 10 }\n";
    out << "  _ => { <| $sum + x }\n";
    out << "})\n";
    out << "[1, 2, 3] >> #(v) => {\n";
    out << "  pulse(v)\n";
    out << "  >_(out)\n";
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
    out << "# pulse = (x) => @[sum = 0](out = [...])\n";
    out << "[1, 2] >> #(v) => {\n";
    out << "  pulse(v)\n";
    out << "  >_(out)\n";
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
  EXPECT_NE(result.stdout_text.find("No Statement Starts With This"), std::string::npos);
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
    fs::path(STYIO_SOURCE_DIR) / "sample" / "algorithms" / "bubble_sort.styio";
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
    "true\n[true,false]\nLovelace\n[\"Ada\",\"Lovelace\"]\n3.500000\n[3.500000,2.000000]\n");

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
    "[1,2,3]\n[[1,2,3],[4,5]]\n{\"x\":1}\n[{\"x\":1},{\"y\":2}]\n[1,2,3]\n[4,5]\n");

  fs::remove(input);
}
