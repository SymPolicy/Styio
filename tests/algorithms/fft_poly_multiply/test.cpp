#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct PolyInput {
  std::vector<int> a;
  std::vector<int> b;
};

PolyInput
test_fft_poly_multiply_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 8);
  std::uniform_int_distribution<int> c_dist(-3, 3);
  PolyInput input;
  input.a.resize(static_cast<std::size_t>(n_dist(rng)));
  input.b.resize(static_cast<std::size_t>(n_dist(rng)));
  for (int& x : input.a) {
    x = c_dist(rng);
  }
  for (int& x : input.b) {
    x = c_dist(rng);
  }
  return input;
}

std::filesystem::path
test_fft_poly_multiply_styio() {
  return styio::testing::algorithms::styio_program(
    "fft_poly_multiply", "fft_poly_multiply.styio");
}

std::string
format_fft_poly_multiply_input(const PolyInput& input) {
  const int n = static_cast<int>(input.a.size());
  const int m = static_cast<int>(input.b.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.push_back(m);
  encoded.insert(encoded.end(), input.a.begin(), input.a.end());
  encoded.insert(encoded.end(), input.b.begin(), input.b.end());
  for (int i = 0; i < n + m; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

std::string
multiline_coeffs(const std::vector<int>& coeffs) {
  std::string out;
  for (int x : coeffs) {
    out += std::to_string(x);
    out += "\n";
  }
  return out;
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_fft_poly_multiply) {
  std::mt19937 rng(0xFF7);
  for (int iteration = 0; iteration < 100; ++iteration) {
    const PolyInput input = test_fft_poly_multiply_random_input(rng);
    const std::vector<int> prod = test_fft_poly_multiply_cpp(input.a, input.b);
    const std::string expected = multiline_coeffs(prod);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_fft_poly_multiply_styio(), format_fft_poly_multiply_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_fft_poly_multiply_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_fft_poly_multiply_fixed_cases) {
  // (1+2x)*(3+4x) = 3 + 10x + 8x^2
  const std::string classic = "[2,2,1,2,3,4,0,0,0,0]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "3\n10\n8\n" },
    { "[0,1]\n", "" },
    { "[]\n", "" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_fft_poly_multiply_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
