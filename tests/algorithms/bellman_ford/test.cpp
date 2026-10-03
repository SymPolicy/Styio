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

struct BellmanFordInput {
  int n = 0;
  int s = 0;
  int t = 0;
  std::vector<std::tuple<int, int, int>> edges;
};

BellmanFordInput
test_bellman_ford_random_input(std::mt19937& rng) {
  // Generate a DAG layered by vertex id so no cycles (hence no neg cycles).
  std::uniform_int_distribution<int> n_dist(1, 10);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  std::uniform_int_distribution<int> m_dist(0, n * 2);
  std::uniform_int_distribution<int> w_dist(-3, 9);

  BellmanFordInput input;
  input.n = n;
  input.s = v_dist(rng);
  input.t = v_dist(rng);
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m));
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
test_bellman_ford_styio() {
  return styio::testing::algorithms::styio_program("bellman_ford", "bellman_ford.styio");
}

std::string
format_bellman_ford_input(const BellmanFordInput& input) {
  std::vector<int> encoded;
  encoded.reserve(4 + input.edges.size() * 3 + static_cast<std::size_t>(input.n));
  encoded.push_back(input.n);
  encoded.push_back(static_cast<int>(input.edges.size()));
  encoded.push_back(input.s);
  encoded.push_back(input.t);
  for (const auto& [u, v, w] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
    encoded.push_back(w);
  }
  for (int i = 0; i < input.n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_bellman_ford) {
  std::mt19937 rng(0xBE11A1);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const BellmanFordInput input = test_bellman_ford_random_input(rng);
    const std::string expected =
      test_bellman_ford_cpp_output(input.n, input.edges, input.s, input.t);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_bellman_ford_styio(), format_bellman_ford_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_bellman_ford_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_bellman_ford_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    // n=1
    { "[1,0,0,0,0]\n", "0\n" },
    // 0->1 w=2, 1->2 w=3, s=0,t=2
    { "[3,2,0,2,0,1,2,1,2,3,0,0,0]\n", "5\n" },
    // negative edge DAG: 0->2:10, 0->1:1, 1->2:-2 => dist -1
    { "[3,3,0,2,0,2,10,0,1,1,1,2,-2,0,0,0]\n", "-1\n" },
    // unreachable -> INF
    { "[3,1,0,2,0,1,4,0,0,0]\n", "1000000000\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(test_bellman_ford_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
