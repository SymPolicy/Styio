#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct CountValueInput
{
  std::vector<int> values;
  int target = 0;
};

CountValueInput
test_count_value_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 56);
  std::uniform_int_distribution<int> value_dist(-20, 20);
  std::bernoulli_distribution present_dist(0.65);

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
test_count_value_styio() {
  return styio::testing::algorithms::styio_program(
    "count_value", "count_value.styio");
}

std::string
format_count_value_input(const CountValueInput& input) {
  std::vector<int> encoded;
  encoded.reserve(input.values.size() + 1);
  encoded.push_back(input.target);
  encoded.insert(encoded.end(), input.values.begin(), input.values.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_count_value) {
  std::mt19937 rng(0xC0017);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const CountValueInput input = test_count_value_random_input(rng);
    const std::string expected =
      test_count_value_cpp_output(input.values, input.target);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_count_value_styio(), format_count_value_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_count_value_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_count_value_malformed_input) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[]\n", "0\n" },
    { "[4]\n", "0\n" },
    { "[4,4]\n", "1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_count_value_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
