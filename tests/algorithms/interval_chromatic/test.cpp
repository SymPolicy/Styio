#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct IcInput {
  std::vector<std::pair<int, int>> intervals;
};

IcInput
test_interval_chromatic_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 12);
  std::uniform_int_distribution<int> t_dist(0, 30);
  IcInput input;
  const int n = n_dist(rng);
  input.intervals.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    const int a = t_dist(rng);
    const int b = t_dist(rng);
    if (a < b) {
      input.intervals.push_back({a, b});
    } else if (b < a) {
      input.intervals.push_back({b, a});
    } else {
      input.intervals.push_back({a, a + 1});
    }
  }
  return input;
}

std::filesystem::path
test_interval_chromatic_styio() {
  return styio::testing::algorithms::styio_program("interval_chromatic",
                                                   "interval_chromatic.styio");
}

std::string
format_interval_chromatic_input(const IcInput& input) {
  const int n = static_cast<int>(input.intervals.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(1 + 2 * n));
  encoded.push_back(n);
  for (const auto& [s, f] : input.intervals) {
    encoded.push_back(s);
  }
  for (const auto& [s, f] : input.intervals) {
    encoded.push_back(f);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_interval_chromatic) {
  std::mt19937 rng(0x1C70);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const IcInput input = test_interval_chromatic_random_input(rng);
    const std::string expected =
      test_interval_chromatic_cpp_output(input.intervals);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_interval_chromatic_styio(), format_interval_chromatic_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_interval_chromatic_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_interval_chromatic_fixed_cases) {
  // [0,2),[1,3),[2,4) => max depth 2 at [1,2)
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[3,0,1,2,2,3,4]\n", "2\n" },
    { "[0]\n", "0\n" },
    { "[1,5,5]\n", "0\n" }, // s>=f ignored
    { "[1,5,8]\n", "1\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_interval_chromatic_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
