#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct BinInput {
  int n = 0;
  int k = 0;
};

BinInput
test_binomial_coefficient_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 25);
  BinInput input;
  input.n = n_dist(rng);
  std::uniform_int_distribution<int> k_dist(0, input.n);
  input.k = k_dist(rng);
  return input;
}

std::filesystem::path
test_binomial_coefficient_styio() {
  return styio::testing::algorithms::styio_program(
    "binomial_coefficient", "binomial_coefficient.styio");
}

std::string
format_binomial_coefficient_input(const BinInput& input) {
  std::vector<int> encoded;
  encoded.push_back(input.n);
  encoded.push_back(input.k);
  for (int i = 0; i < input.k + 1; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_binomial_coefficient) {
  std::mt19937 rng(0xB1A0);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const BinInput input = test_binomial_coefficient_random_input(rng);
    const std::string expected =
      test_binomial_coefficient_cpp_output(input.n, input.k);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_binomial_coefficient_styio(), format_binomial_coefficient_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_binomial_coefficient_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_binomial_coefficient_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[5,2,0,0,0]\n", "10\n" },
    { "[0,0,0]\n", "1\n" },
    { "[3,5]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_binomial_coefficient_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
