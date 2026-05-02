#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>

namespace {

int
test_factorial_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> value_dist(0, 12);
  return value_dist(rng);
}

std::filesystem::path
test_factorial_styio() {
  return styio::testing::algorithms::styio_program("factorial", "factorial.styio");
}

std::string
format_factorial_input(int n) {
  return styio::testing::algorithms::format_i32_list({ n }) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_factorial) {
  std::mt19937 rng(0xFAC701);

  for (int iteration = 0; iteration < 120; ++iteration) {
    const int input = test_factorial_random_input(rng);
    const std::string expected = test_factorial_cpp_output(input);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_factorial_styio(), format_factorial_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_factorial_input(input)
      << "stderr=" << actual.stderr_text;
  }
}
