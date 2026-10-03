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
test_modular_inverse_styio() {
  return styio::testing::algorithms::styio_program("modular_inverse",
                                                   "modular_inverse.styio");
}

std::string
format_modular_inverse_input(int a, int m) {
  return styio::testing::algorithms::format_i32_list({a, m}) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_modular_inverse) {
  std::mt19937 rng(0xA0D1);
  std::uniform_int_distribution<int> a_dist(-40, 40);
  std::uniform_int_distribution<int> m_dist(1, 97);

  for (int iteration = 0; iteration < 200; ++iteration) {
    const int a = a_dist(rng);
    const int m = m_dist(rng);
    const std::string expected = test_modular_inverse_cpp_output(a, m);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_modular_inverse_styio(), format_modular_inverse_input(a, m));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_modular_inverse_input(a, m)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_modular_inverse_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[3,11]\n", "4\n" }, // 3*4=12≡1
    { "[2,5]\n", "3\n" },
    { "[2,4]\n", "-1\n" },
    { "[5,0]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_modular_inverse_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
