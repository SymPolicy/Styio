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
test_euler_totient_styio() {
  return styio::testing::algorithms::styio_program("euler_totient", "euler_totient.styio");
}

std::string
format_euler_totient_input(int n) {
  return styio::testing::algorithms::format_i32_list({n}) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_euler_totient) {
  std::mt19937 rng(0xE01);
  std::uniform_int_distribution<int> n_dist(1, 5000);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const int n = n_dist(rng);
    const std::string expected = test_euler_totient_cpp_output(n);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_euler_totient_styio(), format_euler_totient_input(n));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_euler_totient_input(n);
  }
}

TEST(StyioCppReferenceEquivalence, test_euler_totient_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1]\n", "1\n" },
    { "[9]\n", "6\n" },
    { "[10]\n", "4\n" },
    { "[0]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_euler_totient_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
