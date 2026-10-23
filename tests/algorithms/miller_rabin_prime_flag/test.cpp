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
test_miller_rabin_prime_flag_styio() {
  return styio::testing::algorithms::styio_program(
    "miller_rabin_prime_flag", "miller_rabin_prime_flag.styio");
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_miller_rabin_prime_flag) {
  std::mt19937 rng(0xA811);
  std::uniform_int_distribution<int> n_dist(0, 20000);
  for (int iteration = 0; iteration < 200; ++iteration) {
    const int n = n_dist(rng);
    const std::string expected = test_miller_rabin_prime_flag_cpp_output(n);
    const std::string stdin_text =
      styio::testing::algorithms::format_i32_list({n}) + "\n";
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_miller_rabin_prime_flag_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << "n=" << n;
  }
}

TEST(StyioCppReferenceEquivalence, test_miller_rabin_prime_flag_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[97]\n", "1\n" },
    { "[1]\n", "0\n" },
    { "[9]\n", "0\n" },
    { "[-3]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_miller_rabin_prime_flag_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
