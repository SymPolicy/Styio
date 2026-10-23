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
test_aho_corasick_match_count_styio() {
  return styio::testing::algorithms::styio_program(
    "aho_corasick_match_count", "aho_corasick_match_count.styio");
}

std::string
format_aho_input(const std::vector<int>& text, const std::vector<std::vector<int>>& pats) {
  std::vector<int> encoded;
  encoded.push_back(static_cast<int>(text.size()));
  encoded.push_back(static_cast<int>(pats.size()));
  encoded.insert(encoded.end(), text.begin(), text.end());
  for (const auto& p : pats) {
    encoded.push_back(static_cast<int>(p.size()));
    encoded.insert(encoded.end(), p.begin(), p.end());
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_aho_corasick_match_count) {
  std::mt19937 rng(0xAC01);
  std::uniform_int_distribution<int> n_dist(0, 20);
  std::uniform_int_distribution<int> k_dist(0, 4);
  std::uniform_int_distribution<int> sym(0, 5);
  std::uniform_int_distribution<int> m_dist(1, 4);
  for (int iteration = 0; iteration < 80; ++iteration) {
    const int n = n_dist(rng);
    const int k = k_dist(rng);
    std::vector<int> text(static_cast<std::size_t>(n));
    for (int& x : text) {
      x = sym(rng);
    }
    std::vector<std::vector<int>> pats(static_cast<std::size_t>(k));
    for (auto& p : pats) {
      p.resize(static_cast<std::size_t>(m_dist(rng)));
      for (int& x : p) {
        x = sym(rng);
      }
    }
    const std::string expected = test_aho_corasick_match_count_cpp_output(text, pats);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_aho_corasick_match_count_styio(), format_aho_input(text, pats));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_aho_input(text, pats);
  }
}

TEST(StyioCppReferenceEquivalence, test_aho_corasick_match_count_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0]\n", "0\n" },
    { "[3,1,1,2,1,1,1]\n", "2\n" },
    { "[4,2,1,2,1,2,1,1,2,2,1]\n", "3\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_aho_corasick_match_count_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
