#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct FenwickInput {
  std::vector<int> a;
  int L = 0;
  int R = 0;
};

FenwickInput
test_fenwick_range_sum_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 24);
  std::uniform_int_distribution<int> v_dist(-5, 9);
  FenwickInput input;
  const int n = n_dist(rng);
  input.a.resize(static_cast<std::size_t>(n));
  for (int& x : input.a) {
    x = v_dist(rng);
  }
  std::uniform_int_distribution<int> idx(0, n - 1);
  input.L = idx(rng);
  input.R = idx(rng);
  if (input.L > input.R) {
    std::swap(input.L, input.R);
  }
  return input;
}

std::filesystem::path
test_fenwick_range_sum_styio() {
  return styio::testing::algorithms::styio_program(
    "fenwick_range_sum", "fenwick_range_sum.styio");
}

std::string
format_fenwick_range_sum_input(const FenwickInput& input) {
  const int n = static_cast<int>(input.a.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.push_back(input.L);
  encoded.push_back(input.R);
  encoded.insert(encoded.end(), input.a.begin(), input.a.end());
  for (int i = 0; i < n + 1; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_fenwick_range_sum) {
  std::mt19937 rng(0xFE11);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const FenwickInput input = test_fenwick_range_sum_random_input(rng);
    const std::string expected =
      test_fenwick_range_sum_cpp_output(input.a, input.L, input.R);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_fenwick_range_sum_styio(), format_fenwick_range_sum_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_fenwick_range_sum_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_fenwick_range_sum_fixed_cases) {
  // a=[1,2,3,4], L=1,R=2 -> 2+3=5; workspace 5 zeros
  const std::string classic = "[4,1,2,1,2,3,4,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "5\n" },
    { "[0,0,0]\n", "0\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_fenwick_range_sum_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
