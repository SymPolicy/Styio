#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct LcsLengthInput
{
  std::vector<int> a;
  std::vector<int> b;
};

LcsLengthInput
test_lcs_length_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 10);
  std::uniform_int_distribution<int> v_dist(0, 5);
  const int n = n_dist(rng);
  const int m = n_dist(rng);
  LcsLengthInput input;
  input.a.resize(static_cast<std::size_t>(n));
  input.b.resize(static_cast<std::size_t>(m));
  for (int& x : input.a) {
    x = v_dist(rng);
  }
  for (int& x : input.b) {
    x = v_dist(rng);
  }
  return input;
}

std::filesystem::path
test_lcs_length_styio() {
  return styio::testing::algorithms::styio_program("lcs_length", "lcs_length.styio");
}

std::string
format_lcs_length_input(const LcsLengthInput& input) {
  // Encoding: [n, m, a1..an, b1..bm, then (n+1)*(m+1) DP workspace zeros]
  const int n = static_cast<int>(input.a.size());
  const int m = static_cast<int>(input.b.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + n + m + (n + 1) * (m + 1)));
  encoded.push_back(n);
  encoded.push_back(m);
  encoded.insert(encoded.end(), input.a.begin(), input.a.end());
  encoded.insert(encoded.end(), input.b.begin(), input.b.end());
  for (int i = 0; i < (n + 1) * (m + 1); ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_lcs_length) {
  std::mt19937 rng(0x1C514E);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const LcsLengthInput input = test_lcs_length_random_input(rng);
    const std::string expected = test_lcs_length_cpp_output(input.a, input.b);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_lcs_length_styio(), format_lcs_length_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_lcs_length_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_lcs_length_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0,0]\n", "0\n" },
    // A=ABCBDAB B=BDCABA -> LCS len 4; workspace (8*7)=56 zeros omitted in short form:
    // require full workspace: n=7,m=6 -> 8*7=56
    { "[]\n", "0\n" },
    { "[1,1,7,7,0,0,0,0]\n", "1\n" },
    { "[2,2,1,2,1,3,0,0,0,0,0,0,0,0,0]\n", "1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_lcs_length_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
