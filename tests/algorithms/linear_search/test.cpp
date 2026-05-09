#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct LinearSearchInput
{
  std::vector<int> values;
  int target = 0;
};

LinearSearchInput
test_linear_search_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 36);
  std::uniform_int_distribution<int> value_dist(-300, 300);
  std::bernoulli_distribution present_dist(0.55);

  std::vector<int> values(static_cast<std::size_t>(size_dist(rng)));
  for (int& value : values) {
    value = value_dist(rng);
  }

  int target = value_dist(rng);
  if (!values.empty() && present_dist(rng)) {
    std::uniform_int_distribution<std::size_t> index_dist(0, values.size() - 1);
    target = values[index_dist(rng)];
  }

  return { values, target };
}

std::filesystem::path
test_linear_search_styio() {
  return styio::testing::algorithms::styio_program(
    "linear_search", "linear_search.styio");
}

std::string
format_linear_search_input(const LinearSearchInput& input) {
  std::vector<int> encoded;
  encoded.reserve(input.values.size() + 1);
  encoded.push_back(input.target);
  encoded.insert(encoded.end(), input.values.begin(), input.values.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_linear_search) {
  std::mt19937 rng(0x1EAF5EA);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const LinearSearchInput input = test_linear_search_random_input(rng);
    const std::string expected =
      test_linear_search_cpp_output(input.values, input.target);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_linear_search_styio(), format_linear_search_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_linear_search_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_linear_search_malformed_input) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[]\n", "-1\n" },
    { "[7]\n", "-1\n" },
    { "[7,5]\n", "-1\n" },
    { "[7,7]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_linear_search_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
