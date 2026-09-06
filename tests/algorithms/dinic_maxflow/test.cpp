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

struct FlowInput {
  int n = 0;
  int s = 0;
  int t = 0;
  std::vector<std::tuple<int, int, int>> edges;
};

FlowInput
test_dinic_maxflow_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(2, 6);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  std::uniform_int_distribution<int> c_dist(0, 8);
  std::uniform_int_distribution<int> m_dist(0, n * 2);
  FlowInput input;
  input.n = n;
  input.s = 0;
  input.t = n - 1;
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    input.edges.push_back({v_dist(rng), v_dist(rng), c_dist(rng)});
  }
  return input;
}

std::filesystem::path
test_dinic_maxflow_styio() {
  return styio::testing::algorithms::styio_program("dinic_maxflow", "dinic_maxflow.styio");
}

std::string
format_dinic_maxflow_input(const FlowInput& input) {
  const int m = static_cast<int>(input.edges.size());
  const int n = input.n;
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(4 + m * 3 + n * n + n + n));
  encoded.push_back(n);
  encoded.push_back(m);
  encoded.push_back(input.s);
  encoded.push_back(input.t);
  for (const auto& [u, v, c] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
    encoded.push_back(c);
  }
  for (int i = 0; i < n * n + n + n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_dinic_maxflow) {
  std::mt19937 rng(0xD1C);
  for (int iteration = 0; iteration < 100; ++iteration) {
    const FlowInput input = test_dinic_maxflow_random_input(rng);
    const std::string expected =
      test_dinic_maxflow_cpp_output(input.n, input.s, input.t, input.edges);
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_dinic_maxflow_styio(), format_dinic_maxflow_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_dinic_maxflow_input(input) << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_dinic_maxflow_fixed_cases) {
  const std::string classic =
    "[4,5,0,3,0,1,3,0,2,2,1,2,1,1,3,2,2,3,3,"
    "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    "0,0,0,0,"
    "0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "5\n" },
    { "[2,0,0,1,0,0,0,0,0,0,0,0]\n", "0\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(test_dinic_maxflow_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << "input=" << stdin_text << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << "input=" << stdin_text;
  }
}
