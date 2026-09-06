#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TopoInput {
  int n = 0;
  std::vector<std::pair<int, int>> edges;
};

TopoInput
test_topo_order_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 8);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  std::uniform_int_distribution<int> m_dist(0, n * 2);
  std::uniform_int_distribution<int> cyclic_dist(0, 4);
  TopoInput input;
  input.n = n;
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m + 1));
  for (int i = 0; i < m; ++i) {
    int u = v_dist(rng);
    int v = v_dist(rng);
    if (u == v) {
      continue;
    }
    // Prefer forward edges (DAG); occasionally allow backward to exercise cycles.
    if (cyclic_dist(rng) != 0 && u > v) {
      std::swap(u, v);
    }
    input.edges.push_back({u, v});
  }
  return input;
}

std::filesystem::path
test_topo_order_styio() {
  return styio::testing::algorithms::styio_program("topo_order", "topo_order.styio");
}

std::string
format_topo_order_input(const TopoInput& input) {
  const int m = static_cast<int>(input.edges.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + m * 2 + 3 * input.n));
  encoded.push_back(input.n);
  encoded.push_back(m);
  for (const auto& [u, v] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
  }
  for (int i = 0; i < 3 * input.n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_topo_order) {
  std::mt19937 rng(0x70A0);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const TopoInput input = test_topo_order_random_input(rng);
    const std::string expected = test_topo_order_cpp_output(input.n, input.edges);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_topo_order_styio(), format_topo_order_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_topo_order_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_topo_order_fixed_cases) {
  // 0->1, 0->2, 1->3, 2->3 => lex order 0,1,2,3
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1,0,0,0,0]\n", "0\n" },
    { "[4,4,0,1,0,2,1,3,2,3,0,0,0,0,0,0,0,0,0,0,0,0]\n", "0\n1\n2\n3\n" },
    { "[2,2,0,1,1,0,0,0,0,0,0,0]\n", "" }, // cycle
    { "[]\n", "" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_topo_order_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
