#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

struct EqualFlagInput
{
  std::vector<int> lhs;
  std::vector<int> rhs;
};

EqualFlagInput
test_equal_flag_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 40);
  std::uniform_int_distribution<int> delta_dist(-4, 4);
  std::uniform_int_distribution<int> value_dist(-50, 50);
  std::bernoulli_distribution equal_dist(0.45);

  std::vector<int> lhs(static_cast<std::size_t>(size_dist(rng)));
  for (int& value : lhs) {
    value = value_dist(rng);
  }

  std::vector<int> rhs;
  if (equal_dist(rng)) {
    rhs = lhs;
  } else {
    const int rhs_size = std::max(0, static_cast<int>(lhs.size()) + delta_dist(rng));
    rhs.resize(static_cast<std::size_t>(rhs_size));
    for (int& value : rhs) {
      value = value_dist(rng);
    }
  }

  return { lhs, rhs };
}

std::filesystem::path
test_equal_flag_styio() {
  return styio::testing::algorithms::styio_program(
    "equal_flag", "equal_flag.styio");
}

std::string
format_equal_flag_input(const EqualFlagInput& input) {
  std::vector<int> encoded;
  encoded.reserve(input.lhs.size() + input.rhs.size() + 2);
  encoded.push_back(static_cast<int>(input.lhs.size()));
  encoded.push_back(static_cast<int>(input.rhs.size()));
  encoded.insert(encoded.end(), input.lhs.begin(), input.lhs.end());
  encoded.insert(encoded.end(), input.rhs.begin(), input.rhs.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_equal_flag) {
  std::mt19937 rng(0xE9A1F);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const EqualFlagInput input = test_equal_flag_random_input(rng);
    const std::string expected =
      test_equal_flag_cpp_output(input.lhs, input.rhs);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_equal_flag_styio(), format_equal_flag_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_equal_flag_input(input)
      << "stderr=" << actual.stderr_text;
  }
}
