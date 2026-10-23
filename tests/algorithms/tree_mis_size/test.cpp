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
test_tree_mis_size_styio() {
  return styio::testing::algorithms::styio_program("tree_mis_size", "tree_mis_size.styio");
}

std::string
format_tree_mis_input(int n, const std::vector<std::pair<int, int>>& edges) {
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.push_back(static_cast<int>(edges.size()));
  for (const auto& [u, v] : edges) {
    encoded.push_back(u);
    encoded.push_back(v);
  }
  for (int i = 0; i < n * n + n + n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_tree_mis_size) {
  std::mt19937 rng(0x4151);
  std::uniform_int_distribution<int> n_dist(1, 12);
  for (int iteration = 0; iteration < 80; ++iteration) {
    const int n = n_dist(rng);
    std::vector<std::pair<int, int>> edges;
    for (int i = 1; i < n; ++i) {
      std::uniform_int_distribution<int> p(0, i - 1);
      edges.push_back({p(rng), i});
    }
    const std::string expected = test_tree_mis_size_cpp_output(n, edges);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_tree_mis_size_styio(), format_tree_mis_input(n, edges));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_tree_mis_input(n, edges);
  }
}

TEST(StyioCppReferenceEquivalence, test_tree_mis_size_fixed_cases) {
  // path 0-1-2 MIS=2; workspace 9+3+3=15 zeros after edges
  const std::string path =
    "[3,2,0,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1,0,0,0,0]\n", "1\n" },
    { path, "2\n" },
    { "[0]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_tree_mis_size_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
