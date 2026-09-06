#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct GSInput {
  int n = 0;
  std::vector<std::vector<int>> men_pref;
  std::vector<std::vector<int>> women_pref;
};

GSInput
test_gale_shapley_matching_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 5);
  GSInput input;
  input.n = n_dist(rng);
  auto random_perm = [&]() {
    std::vector<int> p(static_cast<std::size_t>(input.n));
    std::iota(p.begin(), p.end(), 0);
    std::shuffle(p.begin(), p.end(), rng);
    return p;
  };
  input.men_pref.resize(static_cast<std::size_t>(input.n));
  input.women_pref.resize(static_cast<std::size_t>(input.n));
  for (int i = 0; i < input.n; ++i) {
    input.men_pref[static_cast<std::size_t>(i)] = random_perm();
    input.women_pref[static_cast<std::size_t>(i)] = random_perm();
  }
  return input;
}

std::filesystem::path
test_gale_shapley_matching_styio() {
  return styio::testing::algorithms::styio_program(
    "gale_shapley_matching", "gale_shapley_matching.styio");
}

std::string
format_gale_shapley_matching_input(const GSInput& input) {
  const int n = input.n;
  std::vector<int> encoded;
  encoded.push_back(n);
  for (int i = 0; i < n; ++i) {
    for (int x : input.men_pref[static_cast<std::size_t>(i)]) {
      encoded.push_back(x);
    }
  }
  for (int i = 0; i < n; ++i) {
    for (int x : input.women_pref[static_cast<std::size_t>(i)]) {
      encoded.push_back(x);
    }
  }
  // next + wife + husband + rank
  for (int i = 0; i < n + n + n + n * n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_gale_shapley_matching) {
  std::mt19937 rng(0x6A1E);
  for (int iteration = 0; iteration < 60; ++iteration) {
    const GSInput input = test_gale_shapley_matching_random_input(rng);
    const std::string expected = test_gale_shapley_matching_cpp_output(
      input.n, input.men_pref, input.women_pref);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_gale_shapley_matching_styio(), format_gale_shapley_matching_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_gale_shapley_matching_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_gale_shapley_matching_fixed_cases) {
  // n=2; men: 0 prefers 0,1; 1 prefers 0,1; women: 0 prefers 0,1; 1 prefers 1,0
  // proposing-side: both get first choices? man0-w0, man1-w1
  // workspace: next2+wife2+husb2+rank4 = 10 zeros
  const std::string classic =
    "[2,0,1,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "0\n1\n" },
    { "[0]\n", "" },
    { "[]\n", "" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_gale_shapley_matching_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
