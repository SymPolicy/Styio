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

struct PrimInput {
  int n = 0;
  std::vector<std::tuple<int, int, int>> edges;
};

PrimInput
test_prim_mst_weight_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 8);
  const int n = n_dist(rng);
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  std::uniform_int_distribution<int> m_dist(0, n * 3);
  std::uniform_int_distribution<int> w_dist(0, 20);
  PrimInput input;
  input.n = n;
  const int m = m_dist(rng);
  input.edges.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    input.edges.push_back({v_dist(rng), v_dist(rng), w_dist(rng)});
  }
  return input;
}

std::filesystem::path
test_prim_mst_weight_styio() {
  return styio::testing::algorithms::styio_program("prim_mst_weight", "prim_mst_weight.styio");
}

std::string
format_prim_mst_weight_input(const PrimInput& input) {
  const int m = static_cast<int>(input.edges.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + m * 3 + input.n + input.n));
  encoded.push_back(input.n);
  encoded.push_back(m);
  for (const auto& [u, v, w] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
    encoded.push_back(w);
  }
  for (int i = 0; i < input.n; ++i) {
    encoded.push_back(0);
  }
  for (int i = 0; i < input.n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_prim_mst_weight) {
  std::mt19937 rng(0xA71B2);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const PrimInput input = test_prim_mst_weight_random_input(rng);
    const std::string expected =
      test_prim_mst_weight_cpp_output(input.n, input.edges);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_prim_mst_weight_styio(), format_prim_mst_weight_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_prim_mst_weight_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_prim_mst_weight_fixed_cases) {
  // Triangle 0-1:1, 1-2:2, 0-2:4 => MST weight 3
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1,0,0,0]\n", "0\n" },
    { "[3,3,0,1,1,1,2,2,0,2,4,0,0,0,0,0,0]\n", "3\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_prim_mst_weight_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
