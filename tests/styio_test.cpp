#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "StyioTesting/PipelineCheck.hpp"

namespace fs = std::filesystem;

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif

#ifndef STYIO_COMPILER_EXE
#define STYIO_COMPILER_EXE ""
#endif

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
