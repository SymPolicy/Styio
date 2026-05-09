#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct EqualRangeInput
{
  std::vector<int> values;
  int target = 0;
};

EqualRangeInput
test_equal_range_bounds_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 52);
  std::uniform_int_distribution<int> value_dist(-35, 35);
  std::bernoulli_distribution present_dist(0.60);

  std::vector<int> values(static_cast<std::size_t>(size_dist(rng)));
  for (int& value : values) {
    value = value_dist(rng);
  }
  std::sort(values.begin(), values.end());

  int target = value_dist(rng);
  if (!values.empty() && present_dist(rng)) {
    std::uniform_int_distribution<std::size_t> index_dist(0, values.size() - 1);
    target = values[index_dist(rng)];
  }

  return { values, target };
}

std::filesystem::path
test_equal_range_bounds_styio() {
  return styio::testing::algorithms::styio_program(
    "equal_range_bounds", "equal_range_bounds.styio");
}

std::string
format_equal_range_bounds_input(const EqualRangeInput& input) {
  std::vector<int> encoded;
  encoded.reserve(input.values.size() + 1);
  encoded.push_back(input.target);
  encoded.insert(encoded.end(), input.values.begin(), input.values.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_equal_range_bounds) {
  std::mt19937 rng(0xE9A1A6E);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const EqualRangeInput input = test_equal_range_bounds_random_input(rng);
    const std::string expected =
      test_equal_range_bounds_cpp_output(input.values, input.target);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_equal_range_bounds_styio(),
        format_equal_range_bounds_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_equal_range_bounds_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_equal_range_bounds_malformed_input) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[]\n", "-1\n-1\n" },
    { "[5]\n", "0\n0\n" },
    { "[5,5]\n", "0\n1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_equal_range_bounds_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
