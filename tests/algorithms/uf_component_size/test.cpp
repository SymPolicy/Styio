#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct UFInput {
  int n = 0;
  int x = 0;
  std::vector<std::pair<int, int>> edges;
};

UFInput
test_uf_component_size_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 16);
  UFInput input;
  input.n = n_dist(rng);
  std::uniform_int_distribution<int> v(0, input.n - 1);
  input.x = v(rng);
  std::uniform_int_distribution<int> m_dist(0, input.n * 2);
  const int m = m_dist(rng);
  for (int i = 0; i < m; ++i) {
    input.edges.push_back({v(rng), v(rng)});
  }
  return input;
}

std::filesystem::path
test_uf_component_size_styio() {
  return styio::testing::algorithms::styio_program(
    "uf_component_size", "uf_component_size.styio");
}

std::string
format_uf_component_size_input(const UFInput& input) {
  const int m = static_cast<int>(input.edges.size());
  const int n = input.n;
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.push_back(m);
  encoded.push_back(input.x);
  for (const auto& [u, v] : input.edges) {
    encoded.push_back(u);
    encoded.push_back(v);
  }
  for (int i = 0; i < n + n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_uf_component_size) {
  std::mt19937 rng(0xAF51);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const UFInput input = test_uf_component_size_random_input(rng);
    const std::string expected =
      test_uf_component_size_cpp_output(input.n, input.x, input.edges);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_uf_component_size_styio(), format_uf_component_size_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_uf_component_size_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_uf_component_size_fixed_cases) {
  // n=4, unions 0-1,1-2, x=0 -> size 3; workspace 8 zeros
  const std::string classic = "[4,2,0,0,1,1,2,0,0,0,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "3\n" },
    { "[1,0,0,0,0]\n", "1\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_uf_component_size_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
