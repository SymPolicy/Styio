#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct KmpInput {
  std::vector<int> text;
  std::vector<int> pattern;
};

KmpInput
test_kmp_match_index_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 20);
  std::uniform_int_distribution<int> m_dist(0, 8);
  std::uniform_int_distribution<int> a_dist(0, 4);
  KmpInput input;
  const int n = n_dist(rng);
  const int m = m_dist(rng);
  input.text.reserve(static_cast<std::size_t>(n));
  input.pattern.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < n; ++i) {
    input.text.push_back(a_dist(rng));
  }
  for (int i = 0; i < m; ++i) {
    input.pattern.push_back(a_dist(rng));
  }
  return input;
}

std::filesystem::path
test_kmp_match_index_styio() {
  return styio::testing::algorithms::styio_program("kmp_match_index",
                                                   "kmp_match_index.styio");
}

std::string
format_kmp_match_index_input(const KmpInput& input) {
  const int n = static_cast<int>(input.text.size());
  const int m = static_cast<int>(input.pattern.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + n + m + m));
  encoded.push_back(n);
  encoded.push_back(m);
  for (int v : input.text) {
    encoded.push_back(v);
  }
  for (int v : input.pattern) {
    encoded.push_back(v);
  }
  for (int i = 0; i < m; ++i) {
    encoded.push_back(0); // pi workspace
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_kmp_match_index) {
  std::mt19937 rng(0xCB32);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const KmpInput input = test_kmp_match_index_random_input(rng);
    const std::string expected =
      test_kmp_match_index_cpp_output(input.text, input.pattern);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_kmp_match_index_styio(), format_kmp_match_index_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_kmp_match_index_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_kmp_match_index_fixed_cases) {
  // text=[1,2,1,2,3], pat=[1,2,3] => index 2; empty pat => 0
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[5,3,1,2,1,2,3,1,2,3,0,0,0]\n", "2\n" },
    { "[3,3,1,2,3,1,2,4,0,0,0]\n", "-1\n" },
    { "[0,0]\n", "0\n" },
    { "[2,0,9,8]\n", "0\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_kmp_match_index_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
