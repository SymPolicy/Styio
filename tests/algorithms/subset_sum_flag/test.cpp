#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct SubsetInput {
  int target = 0;
  std::vector<int> values;
};

SubsetInput
test_subset_sum_flag_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 10);
  std::uniform_int_distribution<int> t_dist(0, 40);
  std::uniform_int_distribution<int> v_dist(1, 15);
  SubsetInput input;
  input.target = t_dist(rng);
  const int n = n_dist(rng);
  input.values.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    input.values.push_back(v_dist(rng));
  }
  return input;
}

std::filesystem::path
test_subset_sum_flag_styio() {
  return styio::testing::algorithms::styio_program("subset_sum_flag", "subset_sum_flag.styio");
}

std::string
format_subset_sum_flag_input(const SubsetInput& input) {
  const int n = static_cast<int>(input.values.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + n + input.target + 1));
  encoded.push_back(n);
  encoded.push_back(input.target);
  for (int v : input.values) {
    encoded.push_back(v);
  }
  for (int i = 0; i <= input.target; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_subset_sum_flag) {
  std::mt19937 rng(0x5B5E7);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const SubsetInput input = test_subset_sum_flag_random_input(rng);
    const std::string expected =
      test_subset_sum_flag_cpp_output(input.target, input.values);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_subset_sum_flag_styio(), format_subset_sum_flag_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_subset_sum_flag_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_subset_sum_flag_fixed_cases) {
  // {3,1,1,2,2,1}, target 5 -> 1
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0,0]\n", "1\n" },
    { "[6,5,3,1,1,2,2,1,0,0,0,0,0,0]\n", "1\n" },
    { "[3,11,1,2,4,0,0,0,0,0,0,0,0,0,0,0]\n", "0\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_subset_sum_flag_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
