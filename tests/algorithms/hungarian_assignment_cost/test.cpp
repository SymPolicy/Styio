#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct HungInput {
  std::vector<std::vector<int>> cost;
};

HungInput
test_hungarian_assignment_cost_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 6);
  std::uniform_int_distribution<int> c_dist(0, 15);
  HungInput input;
  const int n = n_dist(rng);
  input.cost.assign(static_cast<std::size_t>(n), std::vector<int>(static_cast<std::size_t>(n)));
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      input.cost[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = c_dist(rng);
    }
  }
  return input;
}

std::filesystem::path
test_hungarian_assignment_cost_styio() {
  return styio::testing::algorithms::styio_program(
    "hungarian_assignment_cost", "hungarian_assignment_cost.styio");
}

std::string
format_hungarian_assignment_cost_input(const HungInput& input) {
  const int n = static_cast<int>(input.cost.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      encoded.push_back(input.cost[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
    }
  }
  const int nmask = 1 << n;
  for (int i = 0; i < nmask; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_hungarian_assignment_cost) {
  std::mt19937 rng(0x4116);
  for (int iteration = 0; iteration < 80; ++iteration) {
    const HungInput input = test_hungarian_assignment_cost_random_input(rng);
    const std::string expected = test_hungarian_assignment_cost_cpp_output(input.cost);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_hungarian_assignment_cost_styio(), format_hungarian_assignment_cost_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_hungarian_assignment_cost_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_hungarian_assignment_cost_fixed_cases) {
  // [[1,2],[3,4]] -> min(1+4,2+3)=5; workspace 4 zeros
  const std::string classic = "[2,1,2,3,4,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "5\n" },
    { "[0]\n", "0\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_hungarian_assignment_cost_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
