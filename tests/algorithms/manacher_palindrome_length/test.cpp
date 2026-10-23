#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<int>
test_manacher_palindrome_length_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 24);
  std::uniform_int_distribution<int> v_dist(0, 3);
  std::vector<int> a(static_cast<std::size_t>(n_dist(rng)));
  for (int& x : a) {
    x = v_dist(rng);
  }
  return a;
}

std::filesystem::path
test_manacher_palindrome_length_styio() {
  return styio::testing::algorithms::styio_program(
    "manacher_palindrome_length", "manacher_palindrome_length.styio");
}

std::string
format_manacher_palindrome_length_input(const std::vector<int>& a) {
  std::vector<int> encoded;
  encoded.push_back(static_cast<int>(a.size()));
  encoded.insert(encoded.end(), a.begin(), a.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_manacher_palindrome_length) {
  std::mt19937 rng(0xBAA11);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const auto input = test_manacher_palindrome_length_random_input(rng);
    const std::string expected = test_manacher_palindrome_length_cpp_output(input);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_manacher_palindrome_length_styio(), format_manacher_palindrome_length_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_manacher_palindrome_length_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_manacher_palindrome_length_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[5,1,2,3,2,1]\n", "5\n" },
    { "[0]\n", "0\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_manacher_palindrome_length_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
