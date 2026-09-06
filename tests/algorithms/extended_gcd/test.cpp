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
test_extended_gcd_styio() {
  return styio::testing::algorithms::styio_program("extended_gcd", "extended_gcd.styio");
}

std::string
format_extended_gcd_input(int a, int b) {
  return styio::testing::algorithms::format_i32_list({a, b}) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_extended_gcd) {
  std::mt19937 rng(0xE6CD);
  std::uniform_int_distribution<int> v_dist(-40, 40);

  for (int iteration = 0; iteration < 200; ++iteration) {
    const int a = v_dist(rng);
    const int b = v_dist(rng);
    const std::string expected = test_extended_gcd_cpp_output(a, b);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_extended_gcd_styio(), format_extended_gcd_input(a, b));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_extended_gcd_input(a, b)
      << "stderr=" << actual.stderr_text;

    const auto [g, x, y] = test_extended_gcd_cpp(a, b);
    EXPECT_EQ(static_cast<long long>(a) * x + static_cast<long long>(b) * y, g);
  }
}

TEST(StyioCppReferenceEquivalence, test_extended_gcd_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> fixed = {
    { "[240,46]\n", test_extended_gcd_cpp_output(240, 46) },
    { "[0,0]\n", test_extended_gcd_cpp_output(0, 0) },
    { "[15,-25]\n", test_extended_gcd_cpp_output(15, -25) },
    { "[]\n", "0\n0\n0\n" },
  };

  for (const auto& [stdin_text, expected] : fixed) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_extended_gcd_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
