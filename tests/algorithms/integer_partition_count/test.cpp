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
test_integer_partition_count_styio() {
  return styio::testing::algorithms::styio_program(
    "integer_partition_count", "integer_partition_count.styio");
}

std::string
format_integer_partition_count_input(int n) {
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(1 + n + 1));
  encoded.push_back(n);
  for (int i = 0; i <= n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_integer_partition_count) {
  std::mt19937 rng(0xB477);
  std::uniform_int_distribution<int> n_dist(0, 35);

  for (int iteration = 0; iteration < 80; ++iteration) {
    const int n = n_dist(rng);
    const std::string expected = test_integer_partition_count_cpp_output(n);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_integer_partition_count_styio(),
        format_integer_partition_count_input(n));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_integer_partition_count_input(n)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_integer_partition_count_fixed_cases) {
  // p(5)=7, p(0)=1; need workspace zeros after n
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0]\n", "1\n" },
    { "[5,0,0,0,0,0,0]\n", "7\n" },
    { "[1,0,0]\n", "1\n" },
    { "[-1]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_integer_partition_count_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
