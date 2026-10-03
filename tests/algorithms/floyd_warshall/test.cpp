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

struct FloydWarshallInput {
  int n = 0;
  int s = 0;
  int t = 0;
  std::vector<std::tuple<int, int, int>> edges;
};

FloydWarshallInput
test_floyd_warshall_random_input(std::mt19937& rng) {
  // Forward-only edges on vertex ids => DAG, no neg cycles.
  std::uniform_int_distribution<int> n_dist(1, 8);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  std::uniform_int_distribution<int> m_dist(0, n * 2);
  std::uniform_int_distribution<int> w_dist(-2, 8);

  FloydWarshallInput input;
  input.n = n;
  input.s = v_dist(rng);
  input.t = v_dist(rng);
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
test_floyd_warshall_styio() {
  return styio::testing::algorithms::styio_program("floyd_warshall", "floyd_warshall.styio");
}

std::string
format_floyd_warshall_input(const FloydWarshallInput& input) {
  std::vector<int> encoded;
  encoded.reserve(4 + input.edges.size() * 3 + static_cast<std::size_t>(input.n * input.n));
  encoded.push_back(input.n);
  encoded.push_back(static_cast<int>(input.edges.size()));
  encoded.push_back(input.s);
  encoded.push_back(input.t);
  for (const auto& [u, v, w] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
    encoded.push_back(w);
  }
  for (int i = 0; i < input.n * input.n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_floyd_warshall) {
  std::mt19937 rng(0xF10FD);

  for (int iteration = 0; iteration < 120; ++iteration) {
    const FloydWarshallInput input = test_floyd_warshall_random_input(rng);
    const std::string expected =
      test_floyd_warshall_cpp_output(input.n, input.edges, input.s, input.t);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_floyd_warshall_styio(), format_floyd_warshall_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_floyd_warshall_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_floyd_warshall_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1,0,0,0,0]\n", "0\n" },
    // 0->1:2, 1->2:3 => 5; matrix 9 zeros
    { "[3,2,0,2,0,1,2,1,2,3,0,0,0,0,0,0,0,0,0]\n", "5\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(test_floyd_warshall_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
