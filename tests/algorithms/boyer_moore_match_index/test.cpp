#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct BMInput {
  std::vector<int> text;
  std::vector<int> pattern;
};

BMInput
test_boyer_moore_match_index_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 24);
  std::uniform_int_distribution<int> m_dist(0, 8);
  std::uniform_int_distribution<int> v_dist(0, 7);
  BMInput input;
  input.text.resize(static_cast<std::size_t>(n_dist(rng)));
  input.pattern.resize(static_cast<std::size_t>(m_dist(rng)));
  for (int& x : input.text) x = v_dist(rng);
  for (int& x : input.pattern) x = v_dist(rng);
  return input;
}

std::filesystem::path
test_boyer_moore_match_index_styio() {
  return styio::testing::algorithms::styio_program(
    "boyer_moore_match_index", "boyer_moore_match_index.styio");
}

std::string
format_boyer_moore_match_index_input(const BMInput& input) {
  std::vector<int> encoded;
  encoded.push_back(static_cast<int>(input.text.size()));
  encoded.push_back(static_cast<int>(input.pattern.size()));
  encoded.insert(encoded.end(), input.text.begin(), input.text.end());
  encoded.insert(encoded.end(), input.pattern.begin(), input.pattern.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_boyer_moore_match_index) {
  std::mt19937 rng(0xB0E8);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const BMInput input = test_boyer_moore_match_index_random_input(rng);
    const std::string expected =
      test_boyer_moore_match_index_cpp_output(input.text, input.pattern);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_boyer_moore_match_index_styio(), format_boyer_moore_match_index_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_boyer_moore_match_index_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_boyer_moore_match_index_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[5,2,1,2,3,2,3,2,3]\n", "1\n" },
    { "[0,0]\n", "0\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_boyer_moore_match_index_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
