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
test_stoer_wagner_mincut_styio() {
  return styio::testing::algorithms::styio_program(
    "stoer_wagner_mincut", "stoer_wagner_mincut.styio");
}

std::string
format_stoer_input(int n, const std::vector<int>& mat) {
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.insert(encoded.end(), mat.begin(), mat.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_stoer_wagner_mincut) {
  std::mt19937 rng(0x57C0);
  std::uniform_int_distribution<int> n_dist(2, 6);
  std::uniform_int_distribution<int> w_dist(0, 5);
  for (int iteration = 0; iteration < 60; ++iteration) {
    const int n = n_dist(rng);
    std::vector<int> mat(static_cast<std::size_t>(n * n), 0);
    for (int i = 0; i < n; ++i) {
      for (int j = i + 1; j < n; ++j) {
        const int w = w_dist(rng);
        mat[static_cast<std::size_t>(i * n + j)] = w;
        mat[static_cast<std::size_t>(j * n + i)] = w;
      }
    }
    const std::string expected = test_stoer_wagner_mincut_cpp_output(n, mat);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_stoer_wagner_mincut_styio(), format_stoer_input(n, mat));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_stoer_input(n, mat);
  }
}

TEST(StyioCppReferenceEquivalence, test_stoer_wagner_mincut_fixed_cases) {
  // triangle weights 1: mincut 2
  const std::string tri = "[3,0,1,1,1,0,1,1,1,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1]\n", "0\n" },
    { "[0]\n", "-1\n" },
    { tri, "2\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_stoer_wagner_mincut_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
