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

struct DijkstraInput {
  int n = 0;
  int s = 0;
  int t = 0;
  std::vector<std::tuple<int, int, int>> edges;
};

DijkstraInput
test_dijkstra_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 10);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  std::uniform_int_distribution<int> m_dist(0, n * 2);
  std::uniform_int_distribution<int> w_dist(0, 9);

  DijkstraInput input;
  input.n = n;
  input.s = v_dist(rng);
  input.t = v_dist(rng);
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    input.edges.push_back({v_dist(rng), v_dist(rng), w_dist(rng)});
  }
  return input;
}

std::filesystem::path
test_dijkstra_styio() {
  return styio::testing::algorithms::styio_program("dijkstra", "dijkstra.styio");
}

std::string
format_dijkstra_input(const DijkstraInput& input) {
  // Encoding: [n, m, s, t, u1,v1,w1, ...]
  // Styio uses local heap/dist arrays; trailing padding is optional and ignored.
  std::vector<int> encoded;
  encoded.reserve(4 + input.edges.size() * 3);
  encoded.push_back(input.n);
  encoded.push_back(static_cast<int>(input.edges.size()));
  encoded.push_back(input.s);
  encoded.push_back(input.t);
  for (const auto& [u, v, w] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
    encoded.push_back(w);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_dijkstra) {
  std::mt19937 rng(0xD14A57);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const DijkstraInput input = test_dijkstra_random_input(rng);
    const std::string expected =
      test_dijkstra_cpp_output(input.n, input.edges, input.s, input.t);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_dijkstra_styio(), format_dijkstra_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_dijkstra_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_dijkstra_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1,0,0,0]\n", "0\n" },
    { "[3,2,0,2,0,1,2,1,2,3]\n", "5\n" },
    { "[3,1,0,2,0,1,4]\n", "1000000000\n" },
    // legacy trailing padding still accepted
    { "[3,2,0,2,0,1,2,1,2,3,0,0,0]\n", "5\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(test_dijkstra_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
