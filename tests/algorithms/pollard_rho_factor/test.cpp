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
test_pollard_rho_factor_styio() {
  return styio::testing::algorithms::styio_program("pollard_rho_factor", "pollard_rho_factor.styio");
}

std::string
format_pollard_rho_factor_input(int n) {
  return styio::testing::algorithms::format_i32_list({n}) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_pollard_rho_factor) {
  std::mt19937 rng(0xB0A11);
  std::uniform_int_distribution<int> n_dist(0, 10000);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const int n = n_dist(rng);
    const std::string expected = test_pollard_rho_factor_cpp_output(n);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_pollard_rho_factor_styio(), format_pollard_rho_factor_input(n));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_pollard_rho_factor_input(n);
  }
}

TEST(StyioCppReferenceEquivalence, test_pollard_rho_factor_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1]\n", "-1\n" },
    { "[0]\n", "-1\n" },
    { "[13]\n", "13\n" },
    { "[91]\n", "7\n" },
    { "[100]\n", "2\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_pollard_rho_factor_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
