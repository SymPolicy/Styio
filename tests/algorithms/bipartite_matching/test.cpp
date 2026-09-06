#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct BmInput {
  int nl = 0;
  int nr = 0;
  std::vector<std::pair<int, int>> edges;
};

BmInput
test_bipartite_matching_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 6);
  BmInput input;
  input.nl = n_dist(rng);
  input.nr = n_dist(rng);
  std::uniform_int_distribution<int> l_dist(0, input.nl - 1);
  std::uniform_int_distribution<int> r_dist(0, input.nr - 1);
  std::uniform_int_distribution<int> m_dist(0, input.nl * input.nr);
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    input.edges.push_back({l_dist(rng), r_dist(rng)});
  }
  return input;
}

std::filesystem::path
test_bipartite_matching_styio() {
  return styio::testing::algorithms::styio_program("bipartite_matching",
                                                   "bipartite_matching.styio");
}

std::string
format_bipartite_matching_input(const BmInput& input) {
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
  for (int i = 0; i < N * N; ++i) {
    encoded.push_back(0);
  }
  for (int i = 0; i < N; ++i) {
    encoded.push_back(0);
  }
  for (int i = 0; i < N; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_bipartite_matching) {
  std::mt19937 rng(0xB1A7);

  for (int iteration = 0; iteration < 100; ++iteration) {
    const BmInput input = test_bipartite_matching_random_input(rng);
    const std::string expected =
      test_bipartite_matching_cpp_output(input.nl, input.nr, input.edges);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_bipartite_matching_styio(), format_bipartite_matching_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_bipartite_matching_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_bipartite_matching_fixed_cases) {
  // nl=2,nr=2, edges 0-0,0-1,1-1 => matching 2; N=6
  const std::string match2 =
    "[2,2,3,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]\n";
  const std::string empty1 =
    "[1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { match2, "2\n" },
    { empty1, "0\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_bipartite_matching_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
