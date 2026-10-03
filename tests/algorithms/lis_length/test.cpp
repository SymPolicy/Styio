#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<int>
test_lis_length_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 16);
  std::uniform_int_distribution<int> value_dist(-20, 20);
  std::vector<int> values(static_cast<std::size_t>(size_dist(rng)));
  for (int& v : values) {
    v = value_dist(rng);
  }
  return values;
}

std::filesystem::path
test_lis_length_styio() {
  return styio::testing::algorithms::styio_program("lis_length", "lis_length.styio");
}

std::string
format_lis_length_input(const std::vector<int>& values) {
  const int n = static_cast<int>(values.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(1 + n + n));
  encoded.push_back(n);
  encoded.insert(encoded.end(), values.begin(), values.end());
  for (int i = 0; i < n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_lis_length) {
  std::mt19937 rng(0x1157E1);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const std::vector<int> input = test_lis_length_random_input(rng);
    const std::string expected = test_lis_length_cpp_output(input);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_lis_length_styio(), format_lis_length_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_lis_length_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_lis_length_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0]\n", "0\n" },
    { "[6,10,22,9,33,21,50,0,0,0,0,0,0]\n", "4\n" }, // 10,22,33,50
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(test_lis_length_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
