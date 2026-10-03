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
test_sieve_prefix_prime_count_styio() {
  return styio::testing::algorithms::styio_program(
    "sieve_prefix_prime_count", "sieve_prefix_prime_count.styio");
}

std::string
format_sieve_prefix_prime_count_input(int n) {
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(n) + 2);
  encoded.push_back(n);
  for (int i = 0; i <= n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_sieve_prefix_prime_count) {
  std::mt19937 rng(0x51E7E);
  std::uniform_int_distribution<int> n_dist(0, 200);
  for (int iteration = 0; iteration < 80; ++iteration) {
    const int n = n_dist(rng);
    const std::string expected = test_sieve_prefix_prime_count_cpp_output(n);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_sieve_prefix_prime_count_styio(), format_sieve_prefix_prime_count_input(n));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_sieve_prefix_prime_count_input(n);
  }
}

TEST(StyioCppReferenceEquivalence, test_sieve_prefix_prime_count_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0]\n", "0\n" },
    { "[1,0,0]\n", "0\n" },
    { "[10,0,0,0,0,0,0,0,0,0,0,0]\n", "4\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_sieve_prefix_prime_count_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
