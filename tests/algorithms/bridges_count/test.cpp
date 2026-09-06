#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct BridgeInput {
  int n = 0;
  std::vector<std::pair<int, int>> edges;
};

BridgeInput
test_bridges_count_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 8);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  std::uniform_int_distribution<int> m_dist(0, n * 2);
  BridgeInput input;
  input.n = n;
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    input.edges.push_back({v_dist(rng), v_dist(rng)});
  }
  return input;
}

std::filesystem::path
test_bridges_count_styio() {
  return styio::testing::algorithms::styio_program("bridges_count", "bridges_count.styio");
}

std::string
format_bridges_count_input(const BridgeInput& input) {
  const int m = static_cast<int>(input.edges.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + m * 2 + input.n));
  encoded.push_back(input.n);
  encoded.push_back(m);
  for (const auto& [u, v] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
  }
  for (int i = 0; i < input.n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_bridges_count) {
  std::mt19937 rng(0xB81D6E);

  for (int iteration = 0; iteration < 120; ++iteration) {
    const BridgeInput input = test_bridges_count_random_input(rng);
    const std::string expected = test_bridges_count_cpp_output(input.n, input.edges);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_bridges_count_styio(), format_bridges_count_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_bridges_count_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_bridges_count_fixed_cases) {
  // Path 0-1-2: both edges are bridges => 2
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1,0,0]\n", "0\n" },
    { "[3,2,0,1,1,2,0,0,0]\n", "2\n" },
    { "[3,3,0,1,1,2,0,2,0,0,0]\n", "0\n" }, // triangle: no bridges
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_bridges_count_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
