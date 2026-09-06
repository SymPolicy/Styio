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
test_chinese_remainder_styio() {
  return styio::testing::algorithms::styio_program("chinese_remainder",
                                                   "chinese_remainder.styio");
}

std::string
format_chinese_remainder_input(int a, int m, int b, int n) {
  return styio::testing::algorithms::format_i32_list({a, m, b, n}) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_chinese_remainder) {
  std::mt19937 rng(0xC87);
  std::uniform_int_distribution<int> mod_dist(1, 40);
  std::uniform_int_distribution<int> res_dist(-20, 60);

  for (int iteration = 0; iteration < 200; ++iteration) {
    const int m = mod_dist(rng);
    const int n = mod_dist(rng);
    const int a = res_dist(rng);
    const int b = res_dist(rng);
    const std::string expected = test_chinese_remainder_cpp_output(a, m, b, n);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_chinese_remainder_styio(),
        format_chinese_remainder_input(a, m, b, n));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_chinese_remainder_input(a, m, b, n)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_chinese_remainder_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[2,3,3,5]\n", "8\n" }, // x≡2 (mod 3), x≡3 (mod 5) => 8
    { "[0,4,1,6]\n", "-1\n" }, // gcd=2 does not divide 1
    { "[1,4,3,6]\n", "9\n" }, // gcd=2 divides 2; lcm=12; 9
    { "[1,0,2,5]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_chinese_remainder_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
