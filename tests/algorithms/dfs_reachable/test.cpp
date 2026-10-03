#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct DfsReachableInput
{
  int n = 0;
  int s = 0;
  int t = 0;
  std::vector<std::pair<int, int>> edges;
};

DfsReachableInput
test_dfs_reachable_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 16);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  std::uniform_int_distribution<int> m_dist(0, n * 3);

  DfsReachableInput input;
  input.n = n;
  input.s = v_dist(rng);
  input.t = v_dist(rng);
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    input.edges.push_back({v_dist(rng), v_dist(rng)});
  }
  return input;
}

std::filesystem::path
test_dfs_reachable_styio() {
  return styio::testing::algorithms::styio_program("dfs_reachable", "dfs_reachable.styio");
}

std::string
format_dfs_reachable_input(const DfsReachableInput& input) {
  // Encoding: [n, m, s, t, u1, v1, ..., um, vm]
  std::vector<int> encoded;
  encoded.reserve(4 + input.edges.size() * 2);
  encoded.push_back(input.n);
  encoded.push_back(static_cast<int>(input.edges.size()));
  encoded.push_back(input.s);
  encoded.push_back(input.t);
  for (const auto& [u, v] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_dfs_reachable) {
  std::mt19937 rng(0xDF5EA1);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const DfsReachableInput input = test_dfs_reachable_random_input(rng);
    const std::string expected =
      test_dfs_reachable_cpp_output(input.n, input.edges, input.s, input.t);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_dfs_reachable_styio(), format_dfs_reachable_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_dfs_reachable_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_dfs_reachable_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1,0,0,0]\n", "1\n" },
    { "[3,2,0,2,0,1,1,2]\n", "1\n" },
    { "[3,1,0,2,0,1]\n", "0\n" },
    { "[]\n", "0\n" },
    { "[2,0,0,1]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_dfs_reachable_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
