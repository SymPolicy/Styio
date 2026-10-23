#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct SccInput {
  int n = 0;
  std::vector<std::pair<int, int>> edges;
};

SccInput
test_scc_count_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 8);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  std::uniform_int_distribution<int> m_dist(0, n * 3);
  SccInput input;
  input.n = n;
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    input.edges.push_back({v_dist(rng), v_dist(rng)});
  }
  return input;
}

std::filesystem::path
test_scc_count_styio() {
  return styio::testing::algorithms::styio_program("scc_count", "scc_count.styio");
}

std::string
format_scc_count_input(const SccInput& input) {
  const int m = static_cast<int>(input.edges.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + m * 2 + input.n * input.n + input.n));
  encoded.push_back(input.n);
  encoded.push_back(m);
  for (const auto& [u, v] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
  }
  for (int i = 0; i < input.n * input.n + input.n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_scc_count) {
  std::mt19937 rng(0x5CC0);

  for (int iteration = 0; iteration < 120; ++iteration) {
    const SccInput input = test_scc_count_random_input(rng);
    const std::string expected = test_scc_count_cpp_output(input.n, input.edges);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_scc_count_styio(), format_scc_count_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_scc_count_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_scc_count_fixed_cases) {
  // Two nodes cycle 0<->1 => 1 SCC; plus isolated would be more
  // 0->1,1->0,2 alone => 2 SCCs. n=3,m=2
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1,0," + std::string(1 + 1, '0') + "]\n", "1\n" }, // fix below
  };
  (void)cases;

  std::vector<int> enc = {3, 2, 0, 1, 1, 0};
  for (int i = 0; i < 3 * 3 + 3; ++i) enc.push_back(0);
  const std::string two = styio::testing::algorithms::format_i32_list(enc) + "\n";

  std::vector<int> one = {1, 0};
  for (int i = 0; i < 1 + 1; ++i) one.push_back(0);
  const std::string single = styio::testing::algorithms::format_i32_list(one) + "\n";

  const std::vector<std::pair<std::string, std::string>> fixed = {
    { single, "1\n" },
    { two, "2\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : fixed) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_scc_count_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
