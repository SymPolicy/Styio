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

struct JohnsonInput {
  int n = 0;
  int s = 0;
  int t = 0;
  std::vector<std::tuple<int, int, int>> edges;
};

JohnsonInput
test_johnson_apsp_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 8);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> w_dist(-3, 9);
  std::uniform_int_distribution<int> m_dist(0, n * 2);
  JohnsonInput input;
  input.n = n;
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  input.s = v_dist(rng);
  input.t = v_dist(rng);
  // Forward DAG edges on ids => no negative cycles.
  const int m = m_dist(rng);
  for (int i = 0; i < m; ++i) {
    int u = v_dist(rng);
    int v = v_dist(rng);
    if (u > v) {
      std::swap(u, v);
    }
    if (u == v) {
      continue;
    }
    input.edges.push_back({u, v, w_dist(rng)});
  }
  return input;
}

std::filesystem::path
test_johnson_apsp_styio() {
  return styio::testing::algorithms::styio_program("johnson_apsp", "johnson_apsp.styio");
}

std::string
format_johnson_apsp_input(const JohnsonInput& input) {
  const int m = static_cast<int>(input.edges.size());
  const int n = input.n;
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.push_back(m);
  encoded.push_back(input.s);
  encoded.push_back(input.t);
  for (const auto& [u, v, w] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
    encoded.push_back(w);
  }
  for (int i = 0; i < n * n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_johnson_apsp) {
  std::mt19937 rng(0xA25B);
  for (int iteration = 0; iteration < 80; ++iteration) {
    const JohnsonInput input = test_johnson_apsp_random_input(rng);
    const std::string expected =
      test_johnson_apsp_cpp_output(input.n, input.s, input.t, input.edges);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_johnson_apsp_styio(), format_johnson_apsp_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_johnson_apsp_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_johnson_apsp_fixed_cases) {
  // n=3 path 0->1 (-1), 1->2 (2); s=0,t=2; dist=-1+2=1; workspace 9 zeros
  const std::string path =
    "[3,2,0,2,0,1,-1,1,2,2,0,0,0,0,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { path, "1\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual =
      styio::testing::algorithms::run_styio_program(test_johnson_apsp_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
