#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

std::vector<int>
test_accumulate_sum_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 64);
  std::uniform_int_distribution<int> value_dist(-200, 200);

  std::vector<int> values(static_cast<std::size_t>(size_dist(rng)));
  for (int& value : values) {
    value = value_dist(rng);
  }
  return values;
}

std::filesystem::path
test_accumulate_sum_styio() {
  return styio::testing::algorithms::styio_program(
    "accumulate_sum", "accumulate_sum.styio");
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_accumulate_sum) {
  std::mt19937 rng(0xACC005);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const std::vector<int> input = test_accumulate_sum_random_input(rng);
    const std::string stdin_text =
      styio::testing::algorithms::format_i32_list(input) + "\n";
    const std::string expected = test_accumulate_sum_cpp_output(input);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_accumulate_sum_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << styio::testing::algorithms::format_i32_list(input)
      << "\nstderr=" << actual.stderr_text;
  }
}
