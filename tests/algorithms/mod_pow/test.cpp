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
test_mod_pow_styio() {
  return styio::testing::algorithms::styio_program("mod_pow", "mod_pow.styio");
}

std::string
format_mod_pow_input(int base, int exp, int mod) {
  return styio::testing::algorithms::format_i32_list({base, exp, mod}) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_mod_pow) {
  std::mt19937 rng(0xA0D9);
  std::uniform_int_distribution<int> b_dist(-20, 40);
  std::uniform_int_distribution<int> e_dist(0, 20);
  std::uniform_int_distribution<int> m_dist(1, 97);

  for (int iteration = 0; iteration < 200; ++iteration) {
    const int base = b_dist(rng);
    const int exp = e_dist(rng);
    const int mod = m_dist(rng);
    const std::string expected = test_mod_pow_cpp_output(base, exp, mod);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_mod_pow_styio(), format_mod_pow_input(base, exp, mod));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_mod_pow_input(base, exp, mod)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_mod_pow_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[3,5,13]\n", "9\n" }, // 243 % 13 = 9
    { "[2,10,1000]\n", "24\n" },
    { "[5,0,7]\n", "1\n" },
    { "[3,4,0]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_mod_pow_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
