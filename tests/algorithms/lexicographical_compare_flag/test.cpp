#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

struct LexicographicalCompareInput
{
  std::vector<int> lhs;
  std::vector<int> rhs;
};

LexicographicalCompareInput
test_lexicographical_compare_flag_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 32);
  std::uniform_int_distribution<int> value_dist(-16, 16);

  std::vector<int> lhs(static_cast<std::size_t>(size_dist(rng)));
  std::vector<int> rhs(static_cast<std::size_t>(size_dist(rng)));
  for (int& value : lhs) {
    value = value_dist(rng);
  }
  for (int& value : rhs) {
    value = value_dist(rng);
  }

  return { lhs, rhs };
}

std::filesystem::path
test_lexicographical_compare_flag_styio() {
  return styio::testing::algorithms::styio_program(
    "lexicographical_compare_flag", "lexicographical_compare_flag.styio");
}

std::string
format_lexicographical_compare_flag_input(
  const LexicographicalCompareInput& input) {
  std::vector<int> encoded;
  encoded.reserve(input.lhs.size() + input.rhs.size() + 2);
  encoded.push_back(static_cast<int>(input.lhs.size()));
  encoded.push_back(static_cast<int>(input.rhs.size()));
  encoded.insert(encoded.end(), input.lhs.begin(), input.lhs.end());
  encoded.insert(encoded.end(), input.rhs.begin(), input.rhs.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_lexicographical_compare_flag) {
  std::mt19937 rng(0x1E71CA1);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const LexicographicalCompareInput input =
      test_lexicographical_compare_flag_random_input(rng);
    const std::string expected =
      test_lexicographical_compare_flag_cpp_output(input.lhs, input.rhs);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_lexicographical_compare_flag_styio(),
        format_lexicographical_compare_flag_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_lexicographical_compare_flag_input(input)
      << "stderr=" << actual.stderr_text;
  }
}
