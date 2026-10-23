#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct SelectIthInput {
  int i = 0;
  std::vector<int> values;
};

SelectIthInput
test_select_ith_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(1, 20);
  std::uniform_int_distribution<int> value_dist(-50, 50);
  SelectIthInput input;
  const int n = size_dist(rng);
  input.values.resize(static_cast<std::size_t>(n));
  for (int& v : input.values) {
    v = value_dist(rng);
  }
  std::uniform_int_distribution<int> i_dist(0, n - 1);
  input.i = i_dist(rng);
  return input;
}

std::filesystem::path
test_select_ith_styio() {
  return styio::testing::algorithms::styio_program("select_ith", "select_ith.styio");
}

std::string
format_select_ith_input(const SelectIthInput& input) {
  std::vector<int> encoded;
  encoded.reserve(input.values.size() + 1);
  encoded.push_back(input.i);
  encoded.insert(encoded.end(), input.values.begin(), input.values.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_select_ith) {
  std::mt19937 rng(0x5E1EC7);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const SelectIthInput input = test_select_ith_random_input(rng);
    const std::string expected = test_select_ith_cpp_output(input.values, input.i);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_select_ith_styio(), format_select_ith_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_select_ith_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_select_ith_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,7]\n", "7\n" },
    { "[2,9,3,7,1,8]\n", "7\n" }, // sorted 1,3,7,8,9 -> index 2 is 7
    { "[0]\n", "-1\n" },
    { "[]\n", "-1\n" },
    { "[5,1,2]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(test_select_ith_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
