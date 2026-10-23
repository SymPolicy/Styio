#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

std::filesystem::path
test_stone_merge_cost_styio() {
  return styio::testing::algorithms::styio_program("stone_merge_cost", "stone_merge_cost.styio");
}

std::string
format_stone_merge_cost_input(const std::vector<int>& a) {
  const int n = static_cast<int>(a.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(1 + n + n * n + n + 1));
  encoded.push_back(n);
  encoded.insert(encoded.end(), a.begin(), a.end());
  for (int i = 0; i < n * n; ++i) {
    encoded.push_back(0);
  }
  for (int i = 0; i <= n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_stone_merge_cost) {
  std::mt19937 rng(0x5701E);
  std::uniform_int_distribution<int> n_dist(0, 8);
  std::uniform_int_distribution<int> a_dist(1, 9);
  for (int iteration = 0; iteration < 80; ++iteration) {
    const int n = n_dist(rng);
    std::vector<int> a(static_cast<std::size_t>(n));
    for (int& x : a) {
      x = a_dist(rng);
    }
    const std::string expected = test_stone_merge_cost_cpp_output(a);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_stone_merge_cost_styio(), format_stone_merge_cost_input(a));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_stone_merge_cost_input(a);
  }
}

TEST(StyioCppReferenceEquivalence, test_stone_merge_cost_fixed_cases) {
  // n=3 piles 1,1,1 -> merge cost 5
  // workspace: 9 dp + 4 pref
  const std::string in3 =
    "[3,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0]\n", "0\n" },
    { "[1,5,0,0,0]\n", "0\n" },
    { in3, "5\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_stone_merge_cost_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
