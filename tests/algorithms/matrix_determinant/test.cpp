#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct MatInput {
  std::vector<std::vector<int>> a;
};

MatInput
test_matrix_determinant_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 5);
  std::uniform_int_distribution<int> v_dist(-3, 3);
  MatInput input;
  const int n = n_dist(rng);
  input.a.assign(static_cast<std::size_t>(n), std::vector<int>(static_cast<std::size_t>(n)));
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      input.a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = v_dist(rng);
    }
  }
  return input;
}

std::filesystem::path
test_matrix_determinant_styio() {
  return styio::testing::algorithms::styio_program(
    "matrix_determinant", "matrix_determinant.styio");
}

std::string
format_matrix_determinant_input(const MatInput& input) {
  const int n = static_cast<int>(input.a.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      encoded.push_back(input.a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
    }
  }
  for (int i = 0; i < n + n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_matrix_determinant) {
  std::mt19937 rng(0xDE71);
  for (int iteration = 0; iteration < 100; ++iteration) {
    const MatInput input = test_matrix_determinant_random_input(rng);
    const std::string expected = test_matrix_determinant_cpp_output(input.a);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_matrix_determinant_styio(), format_matrix_determinant_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_matrix_determinant_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_matrix_determinant_fixed_cases) {
  // [[1,2],[3,4]] det=-2
  const std::string classic = "[2,1,2,3,4,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "-2\n" },
    { "[1,7,0,0]\n", "7\n" },
    { "[0]\n", "0\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_matrix_determinant_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
