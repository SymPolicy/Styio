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
test_topo_unique_flag_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 10);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> m_dist(0, n);
  TopoInput input;
  input.n = n;
  const int m = m_dist(rng);
  // prefer forward edges to keep many DAGs
  for (int i = 0; i < m; ++i) {
    std::uniform_int_distribution<int> a_dist(0, n - 1);
    int u = a_dist(rng);
    int v = a_dist(rng);
    if (u > v) {
      std::swap(u, v);
    }
    if (u != v) {
      input.edges.push_back({u, v});
    }
  }
  return input;
}

std::filesystem::path
test_topo_unique_flag_styio() {
  return styio::testing::algorithms::styio_program(
    "topo_unique_flag", "topo_unique_flag.styio");
}

std::string
format_topo_unique_flag_input(const TopoInput& input) {
  const int m = static_cast<int>(input.edges.size());
  std::vector<int> encoded;
  encoded.push_back(input.n);
  encoded.push_back(m);
  for (const auto& [u, v] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
  }
  for (int i = 0; i < input.n * 2; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_topo_unique_flag) {
  std::mt19937 rng(0x70F0);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const TopoInput input = test_topo_unique_flag_random_input(rng);
    const std::string expected =
      test_topo_unique_flag_cpp_output(input.n, input.edges);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_topo_unique_flag_styio(), format_topo_unique_flag_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_topo_unique_flag_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_topo_unique_flag_fixed_cases) {
  // chain 0->1->2 unique; 0->1, 0->2 non-unique
  const std::string unique = "[3,2,0,1,1,2,0,0,0,0,0,0]\n";
  const std::string nonunique = "[3,2,0,1,0,2,0,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { unique, "1\n" },
    { nonunique, "0\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_topo_unique_flag_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
