#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

std::vector<int>
test_max_element_value_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 60);
  std::uniform_int_distribution<int> value_dist(-300, 300);

  std::vector<int> values(static_cast<std::size_t>(size_dist(rng)));
  for (int& value : values) {
    value = value_dist(rng);
  }
  return values;
}

std::filesystem::path
test_max_element_value_styio(const std::string& file_name) {
  return styio::testing::algorithms::styio_program(
    "max_element_value", file_name);
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_max_element_value) {
  std::mt19937 rng(0xAEE1E17);
  const std::vector<std::string> styio_files = {
    "max_element_value_1.styio",
    "max_element_value_2.styio",
    "max_element_value_3.styio",
  };

  for (int iteration = 0; iteration < 180; ++iteration) {
    const std::vector<int> input = test_max_element_value_random_input(rng);
    const std::string stdin_text =
      styio::testing::algorithms::format_i32_list(input) + "\n";
    const std::string expected = test_max_element_value_cpp_output(input);

    for (const std::string& file_name : styio_files) {
      const styio::testing::algorithms::CommandResult actual =
        styio::testing::algorithms::run_styio_program(
          test_max_element_value_styio(file_name), stdin_text);

      ASSERT_EQ(actual.exit_code, 0) << file_name << "\n" << actual.stderr_text;
      EXPECT_EQ(actual.stdout_text, expected)
        << "file=" << file_name
        << "\ninput=" << styio::testing::algorithms::format_i32_list(input)
        << "\nstderr=" << actual.stderr_text;
    }
  }
}
