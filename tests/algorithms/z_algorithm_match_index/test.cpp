#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct ZInput {
  std::vector<int> text;
  std::vector<int> pattern;
};

ZInput
test_z_algorithm_match_index_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 20);
  std::uniform_int_distribution<int> m_dist(0, 8);
  std::uniform_int_distribution<int> v_dist(0, 4);
  ZInput input;
  input.text.resize(static_cast<std::size_t>(n_dist(rng)));
  input.pattern.resize(static_cast<std::size_t>(m_dist(rng)));
  for (int& x : input.text) {
    x = v_dist(rng);
  }
  for (int& x : input.pattern) {
    x = v_dist(rng);
  }
  return input;
}

std::filesystem::path
test_z_algorithm_match_index_styio() {
  return styio::testing::algorithms::styio_program(
    "z_algorithm_match_index", "z_algorithm_match_index.styio");
}

std::string
format_z_algorithm_match_index_input(const ZInput& input) {
  const int n = static_cast<int>(input.text.size());
  const int m = static_cast<int>(input.pattern.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.push_back(m);
  encoded.insert(encoded.end(), input.text.begin(), input.text.end());
  encoded.insert(encoded.end(), input.pattern.begin(), input.pattern.end());
  for (int i = 0; i < m + 1 + n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_z_algorithm_match_index) {
  std::mt19937 rng(0x5A19);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const ZInput input = test_z_algorithm_match_index_random_input(rng);
    const std::string expected =
      test_z_algorithm_match_index_cpp_output(input.text, input.pattern);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_z_algorithm_match_index_styio(), format_z_algorithm_match_index_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_z_algorithm_match_index_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_z_algorithm_match_index_fixed_cases) {
  // n=5,m=2 text=1,2,3,2,3 pat=2,3; workspace 2+1+5=8 zeros
  const std::string hit =
    "[5,2,1,2,3,2,3,2,3,0,0,0,0,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { hit, "1\n" },
    { "[0,0]\n", "0\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_z_algorithm_match_index_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
