#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct SegInput {
  std::vector<int> a;
  int L = 0;
  int R = 0;
};

SegInput
test_segment_tree_range_sum_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 24);
  std::uniform_int_distribution<int> v_dist(-5, 9);
  SegInput input;
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
test_segment_tree_range_sum_styio() {
  return styio::testing::algorithms::styio_program(
    "segment_tree_range_sum", "segment_tree_range_sum.styio");
}

std::string
format_segment_tree_range_sum_input(const SegInput& input) {
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

TEST(StyioCppReferenceEquivalence, test_segment_tree_range_sum) {
  std::mt19937 rng(0x5E67);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const SegInput input = test_segment_tree_range_sum_random_input(rng);
    const std::string expected =
      test_segment_tree_range_sum_cpp_output(input.a, input.L, input.R);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_segment_tree_range_sum_styio(), format_segment_tree_range_sum_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_segment_tree_range_sum_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_segment_tree_range_sum_fixed_cases) {
  const std::string classic = "[4,1,2,1,2,3,4,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "5\n" },
    { "[0,0,0]\n", "0\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_segment_tree_range_sum_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
