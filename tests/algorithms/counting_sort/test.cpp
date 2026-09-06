#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

std::vector<int>
test_counting_sort_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 24);
  std::uniform_int_distribution<int> value_dist(0, 15);

  std::vector<int> values(static_cast<std::size_t>(size_dist(rng)));
  for (int& value : values) {
    value = value_dist(rng);
  }
  return values;
}

std::filesystem::path
test_counting_sort_styio() {
  return styio::testing::algorithms::styio_program("counting_sort", "counting_sort.styio");
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_counting_sort) {
  std::mt19937 rng(0xC08A17);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const std::vector<int> input = test_counting_sort_random_input(rng);
    const std::string stdin_text =
      styio::testing::algorithms::format_i32_list(input) + "\n";
    const std::string expected = test_counting_sort_cpp_output(input);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(test_counting_sort_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << styio::testing::algorithms::format_i32_list(input)
      << "\nstderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_counting_sort_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[]\n", "[]\n" },
    { "[0]\n", "[0]\n" },
    { "[2,5,3,0,2,3,0,3]\n", "[0,0,2,2,3,3,3,5]\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(test_counting_sort_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
