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
test_sam_distinct_substrings_styio() {
  return styio::testing::algorithms::styio_program(
    "sam_distinct_substrings", "sam_distinct_substrings.styio");
}

std::string
format_sam_input(const std::vector<int>& s) {
  std::vector<int> encoded;
  encoded.push_back(static_cast<int>(s.size()));
  encoded.insert(encoded.end(), s.begin(), s.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_sam_distinct_substrings) {
  std::mt19937 rng(0x5A4D);
  std::uniform_int_distribution<int> n_dist(0, 16);
  std::uniform_int_distribution<int> sym(0, 3);
  for (int iteration = 0; iteration < 80; ++iteration) {
    const int n = n_dist(rng);
    std::vector<int> s(static_cast<std::size_t>(n));
    for (int& x : s) {
      x = sym(rng);
    }
    const std::string expected = test_sam_distinct_substrings_cpp_output(s);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_sam_distinct_substrings_styio(), format_sam_input(s));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_sam_input(s);
  }
}

TEST(StyioCppReferenceEquivalence, test_sam_distinct_substrings_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0]\n", "0\n" },
    { "[1,7]\n", "1\n" },
    { "[3,1,2,1]\n", "5\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_sam_distinct_substrings_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
