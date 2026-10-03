#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

std::filesystem::path
test_mos_range_distinct_styio() {
  return styio::testing::algorithms::styio_program(
    "mos_range_distinct", "mos_range_distinct.styio");
}

std::string
format_mos_input(const std::vector<int>& a, const std::vector<std::pair<int, int>>& qs) {
  std::vector<int> encoded;
  encoded.push_back(static_cast<int>(a.size()));
  encoded.push_back(static_cast<int>(qs.size()));
  encoded.insert(encoded.end(), a.begin(), a.end());
  for (const auto& [l, r] : qs) {
    encoded.push_back(l);
    encoded.push_back(r);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_mos_range_distinct) {
  std::mt19937 rng(0x40D5);
  std::uniform_int_distribution<int> n_dist(1, 20);
  std::uniform_int_distribution<int> q_dist(1, 8);
  std::uniform_int_distribution<int> v_dist(0, 6);
  for (int iteration = 0; iteration < 80; ++iteration) {
    const int n = n_dist(rng);
    const int q = q_dist(rng);
    std::vector<int> a(static_cast<std::size_t>(n));
    for (int& x : a) {
      x = v_dist(rng);
    }
    std::uniform_int_distribution<int> idx(0, n - 1);
    std::vector<std::pair<int, int>> qs;
    for (int i = 0; i < q; ++i) {
      int l = idx(rng);
      int r = idx(rng);
      if (l > r) {
        std::swap(l, r);
      }
      qs.emplace_back(l, r);
    }
    const std::string expected = test_mos_range_distinct_cpp_output(a, qs);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_mos_range_distinct_styio(), format_mos_input(a, qs));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_mos_input(a, qs);
  }
}

TEST(StyioCppReferenceEquivalence, test_mos_range_distinct_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0]\n", "0\n" },
    { "[3,1,1,2,1,0,2]\n", "2\n" },
    { "[4,2,1,1,2,3,0,1,2,3]\n", "3\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_mos_range_distinct_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
