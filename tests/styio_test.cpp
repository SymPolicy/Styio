#include <gtest/gtest.h>

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <fstream>
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

TEST(StyioParserEngine, LegacyAndNewMatchOnM1Sample) {
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
    std::string("\"") + runner + "\" --parser-engine=new --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNewMatchOnM1TypedBindSample) {
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
    std::string("\"") + runner + "\" --parser-engine=new --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNewMatchOnM1CompoundAssignSample) {
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
    std::string("\"") + runner + "\" --parser-engine=new --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNewMatchOnM2SimpleFuncSample) {
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
    std::string("\"") + runner + "\" --parser-engine=new --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  ASSERT_EQ(legacy.exit_code, 0);
  ASSERT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);
}

TEST(StyioParserEngine, LegacyAndNewMatchOnHashExprBodyWithoutArrow) {
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
    std::string("\"") + runner + "\" --parser-engine=new --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 0);
  EXPECT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);

  fs::remove(input);
}

TEST(StyioParserEngine, LegacyAndNewMatchOnHashArrowWithoutAssignment) {
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
    std::string("\"") + runner + "\" --parser-engine=new --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 0);
  EXPECT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);

  fs::remove(input);
}

TEST(StyioParserEngine, LegacyAndNewMatchOnHashMatchCases) {
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
    std::string("\"") + runner + "\" --parser-engine=new --file \"" + input.string() + "\" 2>/dev/null";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 0);
  EXPECT_EQ(newer.exit_code, 0);
  EXPECT_EQ(newer.stdout_text, legacy.stdout_text);

  fs::remove(input);
}

TEST(StyioParserEngine, HashIteratorDefinitionReturnsParseErrorWithoutCrash) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const fs::path input = fs::temp_directory_path() / ("styio-hash-iter-guard-" + std::to_string(uniq) + ".styio");

  {
    std::ofstream out(input);
    ASSERT_TRUE(out.is_open());
    out << "# iter_only(x) >> (n) ?= 2 => >_(n)\n";
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
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=new --file \""
    + input.string() + "\" 2>&1";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 3);
  EXPECT_EQ(newer.exit_code, 3);
  EXPECT_NE(legacy.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);
  EXPECT_NE(newer.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);

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
    std::string("\"") + runner + "\" --parser-engine=new --parser-shadow-compare --file \""
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
      std::string("\"") + runner + "\" --parser-engine=new --parser-shadow-compare --file \""
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
      std::string("\"") + runner + "\" --parser-engine=new --parser-shadow-compare --file \""
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
      std::string("\"") + runner + "\" --parser-engine=new --parser-shadow-compare --file \""
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
      std::string("\"") + runner + "\" --parser-engine=new --parser-shadow-compare --file \""
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
    std::string("\"") + runner + "\" --parser-engine=new --parser-shadow-compare --parser-shadow-artifact-dir \""
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
  EXPECT_NE(line.find("\"primary_engine\":\"new\""), std::string::npos);
  EXPECT_NE(line.find("\"shadow_engine\":\"legacy\""), std::string::npos);

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
    std::string("\"") + runner + "\" --error-format=jsonl --parser-engine=new --file \""
    + input.string() + "\" 2>&1";

  const CommandResult legacy = run_stdout_command(cmd_legacy);
  const CommandResult newer = run_stdout_command(cmd_new);
  EXPECT_EQ(legacy.exit_code, 3);
  EXPECT_EQ(newer.exit_code, 3);
  EXPECT_NE(legacy.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);
  EXPECT_NE(newer.stdout_text.find("\"category\":\"ParseError\""), std::string::npos);

  fs::remove(input);
}
