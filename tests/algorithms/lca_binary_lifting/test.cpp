#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct LcaInput {
  std::vector<int> parent;
  int u = 0;
  int v = 0;
};

LcaInput
test_lca_binary_lifting_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 16);
  const int n = n_dist(rng);
  LcaInput input;
  input.parent.assign(static_cast<std::size_t>(n), -1);
  for (int i = 1; i < n; ++i) {
    std::uniform_int_distribution<int> p_dist(0, i - 1);
    input.parent[static_cast<std::size_t>(i)] = p_dist(rng);
  }
  std::uniform_int_distribution<int> v_dist(0, n - 1);
  input.u = v_dist(rng);
  input.v = v_dist(rng);
  return input;
}

std::filesystem::path
test_lca_binary_lifting_styio() {
  return styio::testing::algorithms::styio_program(
    "lca_binary_lifting", "lca_binary_lifting.styio");
}

std::string
format_lca_binary_lifting_input(const LcaInput& input) {
  const int n = static_cast<int>(input.parent.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.push_back(input.u);
  encoded.push_back(input.v);
  encoded.insert(encoded.end(), input.parent.begin(), input.parent.end());
  for (int i = 0; i < n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_lca_binary_lifting) {
  std::mt19937 rng(0x1CA);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const LcaInput input = test_lca_binary_lifting_random_input(rng);
    const std::string expected =
      test_lca_binary_lifting_cpp_output(input.parent, input.u, input.v);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_lca_binary_lifting_styio(), format_lca_binary_lifting_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_lca_binary_lifting_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_lca_binary_lifting_fixed_cases) {
  // parents: 0<-1,0<-2,1<-3 ; LCA(3,2)=0
  const std::string classic = "[4,3,2,-1,0,0,1,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "0\n" },
    { "[1,0,0,-1,0]\n", "0\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_lca_binary_lifting_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
