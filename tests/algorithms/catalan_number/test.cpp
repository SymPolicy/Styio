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
test_catalan_number_styio() {
  return styio::testing::algorithms::styio_program("catalan_number", "catalan_number.styio");
}

std::string
format_catalan_number_input(int n) {
  std::vector<int> encoded;
  encoded.push_back(n);
  if (n >= 0) {
    for (int i = 0; i <= n; ++i) {
      encoded.push_back(0);
    }
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_catalan_number) {
  std::mt19937 rng(0xCA7A1);
  std::uniform_int_distribution<int> n_dist(0, 12);

  for (int iteration = 0; iteration < 80; ++iteration) {
    const int n = n_dist(rng);
    const std::string expected = test_catalan_number_cpp_output(n);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_catalan_number_styio(), format_catalan_number_input(n));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_catalan_number_input(n)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_catalan_number_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0]\n", "1\n" },
    { "[1,0,0]\n", "1\n" },
    { "[5,0,0,0,0,0,0]\n", "42\n" },
    { "[-1]\n", "0\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_catalan_number_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
