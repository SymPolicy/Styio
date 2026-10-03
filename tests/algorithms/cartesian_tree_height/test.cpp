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
test_cartesian_tree_height_styio() {
  return styio::testing::algorithms::styio_program(
    "cartesian_tree_height", "cartesian_tree_height.styio");
}

std::string
format_cartesian_input(const std::vector<int>& a) {
  const int n = static_cast<int>(a.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.insert(encoded.end(), a.begin(), a.end());
  for (int i = 0; i < 5 * n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_cartesian_tree_height) {
  std::mt19937 rng(0xCA27);
  std::uniform_int_distribution<int> n_dist(0, 20);
  std::uniform_int_distribution<int> v_dist(0, 30);
  for (int iteration = 0; iteration < 100; ++iteration) {
    const int n = n_dist(rng);
    std::vector<int> a(static_cast<std::size_t>(n));
    for (int& x : a) {
      x = v_dist(rng);
    }
    const std::string expected = test_cartesian_tree_height_cpp_output(a);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_cartesian_tree_height_styio(), format_cartesian_input(a));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_cartesian_input(a);
  }
}

TEST(StyioCppReferenceEquivalence, test_cartesian_tree_height_fixed_cases) {
  // [3,1,2] min root=1 at idx1; left=3,right=2; height 1
  // n=3 + 15 workspace
  const std::string ex =
    "[3,3,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0]\n", "0\n" },
    { "[1,5,0,0,0,0,0]\n", "0\n" },
    { ex, "1\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_cartesian_tree_height_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
