#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <string>
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
