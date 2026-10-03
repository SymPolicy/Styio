#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<std::pair<int, int>>
test_activity_selection_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 12);
  std::uniform_int_distribution<int> t_dist(0, 30);
  const int n = n_dist(rng);
  std::vector<std::pair<int, int>> activities;
  activities.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    int a = t_dist(rng);
    int b = t_dist(rng);
    if (a > b) {
      std::swap(a, b);
    }
    if (a == b) {
      b = a + 1;
    }
    activities.push_back({a, b});
  }
  return activities;
}

std::filesystem::path
test_activity_selection_styio() {
  return styio::testing::algorithms::styio_program("activity_selection", "activity_selection.styio");
}

std::string
format_activity_selection_input(const std::vector<std::pair<int, int>>& activities) {
  const int n = static_cast<int>(activities.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(1 + 3 * n));
  encoded.push_back(n);
  for (const auto& [s, f] : activities) {
    encoded.push_back(s);
  }
  for (const auto& [s, f] : activities) {
    encoded.push_back(f);
  }
  for (int i = 0; i < n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_activity_selection) {
  std::mt19937 rng(0xAC71F);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const auto input = test_activity_selection_random_input(rng);
    const std::string expected = test_activity_selection_cpp_output(input);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_activity_selection_styio(), format_activity_selection_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_activity_selection_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_activity_selection_fixed_cases) {
  // CLRS-style: (1,4),(3,5),(0,6),(5,7),(3,9),(5,9),(6,10),(8,11),(8,12),(2,14),(12,16)
  // Optimal count 4 e.g. (1,4),(5,7),(8,11),(12,16)
  const std::string clrs =
    "[11,1,3,0,5,3,5,6,8,8,2,12,4,5,6,7,9,9,10,11,12,14,16,"
    "0,0,0,0,0,0,0,0,0,0,0]\n";

  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0]\n", "0\n" },
    { clrs, "4\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_activity_selection_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
