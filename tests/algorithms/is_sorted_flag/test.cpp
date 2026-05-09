#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

std::vector<int>
test_is_sorted_flag_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 52);
  std::uniform_int_distribution<int> value_dist(-80, 80);
  std::bernoulli_distribution sorted_dist(0.45);

  std::vector<int> values(static_cast<std::size_t>(size_dist(rng)));
  for (int& value : values) {
    value = value_dist(rng);
  }
  if (sorted_dist(rng)) {
    std::sort(values.begin(), values.end());
  }
  return values;
}

std::filesystem::path
test_is_sorted_flag_styio() {
  return styio::testing::algorithms::styio_program(
    "is_sorted_flag", "is_sorted_flag.styio");
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_is_sorted_flag) {
  std::mt19937 rng(0x15027ED);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const std::vector<int> input = test_is_sorted_flag_random_input(rng);
    const std::string stdin_text =
      styio::testing::algorithms::format_i32_list(input) + "\n";
    const std::string expected = test_is_sorted_flag_cpp_output(input);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_is_sorted_flag_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << styio::testing::algorithms::format_i32_list(input)
      << "\nstderr=" << actual.stderr_text;
  }
}
