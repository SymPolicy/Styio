#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TwoSatInput {
  int nv = 0;
  std::vector<std::pair<int, int>> clauses;
};

TwoSatInput
test_two_sat_flag_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> nv_dist(1, 6);
  TwoSatInput input;
  input.nv = nv_dist(rng);
  std::uniform_int_distribution<int> m_dist(0, input.nv * 3);
  std::uniform_int_distribution<int> lit_dist(1, input.nv);
  std::uniform_int_distribution<int> sign(0, 1);
  const int m = m_dist(rng);
  for (int i = 0; i < m; ++i) {
    int a = lit_dist(rng);
    int b = lit_dist(rng);
    if (sign(rng)) a = -a;
    if (sign(rng)) b = -b;
    input.clauses.push_back({a, b});
  }
  return input;
}

std::filesystem::path
test_two_sat_flag_styio() {
  return styio::testing::algorithms::styio_program("two_sat_flag", "two_sat_flag.styio");
}

std::string
format_two_sat_flag_input(const TwoSatInput& input) {
  const int m = static_cast<int>(input.clauses.size());
  const int N = input.nv * 2;
  std::vector<int> encoded;
  encoded.push_back(input.nv);
  encoded.push_back(m);
  for (const auto& [a, b] : input.clauses) {
    encoded.push_back(a);
    encoded.push_back(b);
  }
  for (int i = 0; i < N * N; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_two_sat_flag) {
  std::mt19937 rng(0x25A7);
  for (int iteration = 0; iteration < 100; ++iteration) {
    const TwoSatInput input = test_two_sat_flag_random_input(rng);
    const std::string expected =
      test_two_sat_flag_cpp_output(input.nv, input.clauses);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_two_sat_flag_styio(), format_two_sat_flag_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_two_sat_flag_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_two_sat_flag_fixed_cases) {
  // (x1 \/ x2) /\ (~x1 \/ ~x2) sat; N=4 workspace 16
  std::string ws;
  for (int i = 0; i < 16; ++i) ws += ",0";
  const std::string sat = "[2,2,1,2,-1,-2" + ws + "]\n";
  // x1 /\ ~x1
  const std::vector<std::pair<std::string, std::string>> cases = {
    { sat, "1\n" },
    { "[1,2,1,1,-1,-1,0,0,0,0]\n", "0\n" },
    { "[0,0]\n", "1\n" },
    { "[]\n", "1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_two_sat_flag_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
