#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <random>
#include <string>

namespace {

std::array<int, 2>
test_euclidean_gcd_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> value_dist(-100000, 100000);
  return { value_dist(rng), value_dist(rng) };
}

std::filesystem::path
test_euclidean_gcd_styio() {
  return styio::testing::algorithms::styio_program(
    "euclidean_gcd", "euclidean_gcd.styio");
}

std::string
format_gcd_input(const std::array<int, 2>& input) {
  return styio::testing::algorithms::format_i32_list({ input[0], input[1] }) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_euclidean_gcd) {
  std::mt19937 rng(0x6CD5EED);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const std::array<int, 2> input = test_euclidean_gcd_random_input(rng);
    const std::string expected = test_euclidean_gcd_cpp_output(input[0], input[1]);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_euclidean_gcd_styio(), format_gcd_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_gcd_input(input)
      << "stderr=" << actual.stderr_text;
  }
}
