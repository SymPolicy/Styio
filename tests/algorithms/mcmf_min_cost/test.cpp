#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

std::filesystem::path
test_mcmf_min_cost_styio() {
  return styio::testing::algorithms::styio_program("mcmf_min_cost", "mcmf_min_cost.styio");
}

std::string
format_mcmf_input(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int, int>>& edges) {
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.push_back(static_cast<int>(edges.size()));
  encoded.push_back(s);
  encoded.push_back(t);
  for (const auto& [u, v, c, w] : edges) {
    encoded.push_back(u);
    encoded.push_back(v);
    encoded.push_back(c);
    encoded.push_back(w);
  }
  for (int i = 0; i < n * n + n * n + n + n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_mcmf_min_cost) {
  std::mt19937 rng(0x4CF1);
  std::uniform_int_distribution<int> n_dist(2, 5);
  std::uniform_int_distribution<int> cap_dist(0, 3);
  std::uniform_int_distribution<int> cost_dist(0, 5);
  for (int iteration = 0; iteration < 60; ++iteration) {
    const int n = n_dist(rng);
    std::uniform_int_distribution<int> v_dist(0, n - 1);
    const int s = 0;
    const int t = n - 1;
    const int m = n + 2;
    std::vector<std::tuple<int, int, int, int>> edges;
    for (int i = 0; i < m; ++i) {
      int u = v_dist(rng);
      int v = v_dist(rng);
      if (u == v) {
        continue;
      }
      edges.emplace_back(u, v, cap_dist(rng), cost_dist(rng));
    }
    // ensure a path s->...->t with unit caps
    for (int i = 0; i + 1 < n; ++i) {
      edges.emplace_back(i, i + 1, 1, cost_dist(rng));
    }
    const std::string expected = test_mcmf_min_cost_cpp_output(n, s, t, edges);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_mcmf_min_cost_styio(), format_mcmf_input(n, s, t, edges));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_mcmf_input(n, s, t, edges);
  }
}

TEST(StyioCppReferenceEquivalence, test_mcmf_min_cost_fixed_cases) {
  // 0->1 cap1 cost3, 0->1? simple 0->1: cost 3
  // n=2,m=1,s=0,t=1, edge 0,1,1,3 + workspace 4+4+2+2=12 zeros
  const std::string simple =
    "[2,1,0,1,0,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { simple, "3\n" },
    { "[2,0,0,0]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_mcmf_min_cost_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
