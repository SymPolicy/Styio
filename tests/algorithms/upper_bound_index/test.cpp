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

struct UpperBoundInput
{
  std::vector<int> values;
  int target = 0;
};

UpperBoundInput
test_upper_bound_index_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 52);
  std::uniform_int_distribution<int> value_dist(-70, 70);

  std::vector<int> values(static_cast<std::size_t>(size_dist(rng)));
  for (int& value : values) {
    value = value_dist(rng);
  }
  std::sort(values.begin(), values.end());

  return { values, value_dist(rng) };
}

std::filesystem::path
test_upper_bound_index_styio() {
  return styio::testing::algorithms::styio_program(
    "upper_bound_index", "upper_bound_index.styio");
}

std::string
format_upper_bound_index_input(const UpperBoundInput& input) {
  std::vector<int> encoded;
  encoded.reserve(input.values.size() + 1);
  encoded.push_back(input.target);
  encoded.insert(encoded.end(), input.values.begin(), input.values.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_upper_bound_index) {
  std::mt19937 rng(0x0B0E1D);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const UpperBoundInput input = test_upper_bound_index_random_input(rng);
    const std::string expected =
      test_upper_bound_index_cpp_output(input.values, input.target);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_upper_bound_index_styio(), format_upper_bound_index_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_upper_bound_index_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_upper_bound_index_malformed_input) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[]\n", "-1\n" },
    { "[5]\n", "0\n" },
    { "[5,5]\n", "1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_upper_bound_index_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
