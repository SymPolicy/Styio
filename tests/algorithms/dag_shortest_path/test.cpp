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

struct DagSpInput {
  int n = 0;
  int s = 0;
  int t = 0;
  std::vector<std::tuple<int, int, int>> edges;
};

DagSpInput
test_dag_shortest_path_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 10);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  std::uniform_int_distribution<int> m_dist(0, n * 2);
  std::uniform_int_distribution<int> w_dist(-5, 15);
  DagSpInput input;
  input.n = n;
  input.s = v_dist(rng);
  input.t = v_dist(rng);
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    int u = v_dist(rng);
    int v = v_dist(rng);
    if (u == v) {
      continue;
    }
    if (u > v) {
      std::swap(u, v);
    }
    input.edges.push_back({u, v, w_dist(rng)});
  }
  return input;
}

std::filesystem::path
test_dag_shortest_path_styio() {
  return styio::testing::algorithms::styio_program("dag_shortest_path",
                                                   "dag_shortest_path.styio");
}

std::string
format_dag_shortest_path_input(const DagSpInput& input) {
  const int m = static_cast<int>(input.edges.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(4 + m * 3 + input.n));
  encoded.push_back(input.n);
  encoded.push_back(m);
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

TEST(StyioCppReferenceEquivalence, test_dag_shortest_path) {
  std::mt19937 rng(0xDA651);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const DagSpInput input = test_dag_shortest_path_random_input(rng);
    const std::string expected =
      test_dag_shortest_path_cpp_output(input.n, input.edges, input.s, input.t);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_dag_shortest_path_styio(), format_dag_shortest_path_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_dag_shortest_path_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_dag_shortest_path_fixed_cases) {
  // 0->1:2, 1->2:3, s=0,t=2 => 5; workspace 3 zeros
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1,0,0,0,0]\n", "0\n" },
    { "[3,2,0,2,0,1,2,1,2,3,0,0,0]\n", "5\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_dag_shortest_path_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
