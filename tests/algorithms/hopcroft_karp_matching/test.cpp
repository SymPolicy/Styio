#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct MatchInput {
  int nl = 0;
  int nr = 0;
  std::vector<std::pair<int, int>> edges;
};

MatchInput
test_hopcroft_karp_matching_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 5);
  MatchInput input;
  input.nl = n_dist(rng);
  input.nr = n_dist(rng);
  std::uniform_int_distribution<int> ul(0, input.nl - 1);
  std::uniform_int_distribution<int> ur(0, input.nr - 1);
  std::uniform_int_distribution<int> m_dist(0, input.nl * input.nr);
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    input.edges.push_back({ul(rng), ur(rng)});
  }
  return input;
}

std::filesystem::path
test_hopcroft_karp_matching_styio() {
  return styio::testing::algorithms::styio_program(
    "hopcroft_karp_matching", "hopcroft_karp_matching.styio");
}

std::string
format_hopcroft_karp_matching_input(const MatchInput& input) {
  const int m = static_cast<int>(input.edges.size());
  const int N = input.nl + input.nr + 2;
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(3 + m * 2 + N * N + N + N));
  encoded.push_back(input.nl);
  encoded.push_back(input.nr);
  encoded.push_back(m);
  for (const auto& [u, v] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
  }
  for (int i = 0; i < N * N + N + N; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_hopcroft_karp_matching) {
  std::mt19937 rng(0xC0FFEE);
  for (int iteration = 0; iteration < 100; ++iteration) {
    const MatchInput input = test_hopcroft_karp_matching_random_input(rng);
    const std::string expected =
      test_hopcroft_karp_matching_cpp_output(input.nl, input.nr, input.edges);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_hopcroft_karp_matching_styio(), format_hopcroft_karp_matching_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_hopcroft_karp_matching_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_hopcroft_karp_matching_fixed_cases) {
  // nl=2,nr=2, edges 0-0,0-1,1-1; N=6; need workspace 36+6+6
  std::string ws;
  for (int i = 0; i < 36 + 6 + 6; ++i) {
    ws += ",0";
  }
  const std::string classic = "[2,2,3,0,0,0,1,1,1" + ws + "]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "2\n" },
    { "[0,1,0]\n", "0\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_hopcroft_karp_matching_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << "input=" << stdin_text;
  }
}
