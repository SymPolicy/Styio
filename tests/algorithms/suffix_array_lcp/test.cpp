#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct SaInput {
  std::vector<int> s;
  int i = 0;
  int j = 0;
};

SaInput
test_suffix_array_lcp_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 18);
  std::uniform_int_distribution<int> a_dist(0, 3);
  SaInput input;
  const int n = n_dist(rng);
  input.s.resize(static_cast<std::size_t>(n));
  for (int& x : input.s) {
    x = a_dist(rng);
  }
  std::uniform_int_distribution<int> idx(0, n - 1);
  input.i = idx(rng);
  input.j = idx(rng);
  return input;
}

std::filesystem::path
test_suffix_array_lcp_styio() {
  return styio::testing::algorithms::styio_program(
    "suffix_array_lcp", "suffix_array_lcp.styio");
}

std::string
format_suffix_array_lcp_input(const SaInput& input) {
  const int n = static_cast<int>(input.s.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.push_back(input.i);
  encoded.push_back(input.j);
  encoded.insert(encoded.end(), input.s.begin(), input.s.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_suffix_array_lcp) {
  std::mt19937 rng(0x5A1C);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const SaInput input = test_suffix_array_lcp_random_input(rng);
    const std::string expected =
      test_suffix_array_lcp_cpp_output(input.s, input.i, input.j);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_suffix_array_lcp_styio(), format_suffix_array_lcp_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_suffix_array_lcp_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_suffix_array_lcp_fixed_cases) {
  // s=a b a b a, i=0,j=2 -> "ababa" vs "aba" LCP=3
  const std::string classic = "[5,0,2,1,2,1,2,1]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "3\n" },
    { "[3,1,1,9,8,7]\n", "2\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_suffix_array_lcp_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
