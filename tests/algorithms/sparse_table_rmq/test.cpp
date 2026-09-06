#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct RmqInput {
  std::vector<int> a;
  int L = 0;
  int R = 0;
};

RmqInput
test_sparse_table_rmq_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 24);
  std::uniform_int_distribution<int> v_dist(-20, 20);
  RmqInput input;
  const int n = n_dist(rng);
  input.a.resize(static_cast<std::size_t>(n));
  for (int& x : input.a) {
    x = v_dist(rng);
  }
  std::uniform_int_distribution<int> idx(0, n - 1);
  input.L = idx(rng);
  input.R = idx(rng);
  if (input.L > input.R) {
    std::swap(input.L, input.R);
  }
  return input;
}

std::filesystem::path
test_sparse_table_rmq_styio() {
  return styio::testing::algorithms::styio_program(
    "sparse_table_rmq", "sparse_table_rmq.styio");
}

std::string
format_sparse_table_rmq_input(const RmqInput& input) {
  const int n = static_cast<int>(input.a.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  encoded.push_back(input.L);
  encoded.push_back(input.R);
  encoded.insert(encoded.end(), input.a.begin(), input.a.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_sparse_table_rmq) {
  std::mt19937 rng(0x5A7A);
  for (int iteration = 0; iteration < 120; ++iteration) {
    const RmqInput input = test_sparse_table_rmq_random_input(rng);
    const std::string expected =
      test_sparse_table_rmq_cpp_output(input.a, input.L, input.R);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_sparse_table_rmq_styio(), format_sparse_table_rmq_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_sparse_table_rmq_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_sparse_table_rmq_fixed_cases) {
  const std::string classic = "[5,1,3,9,3,7,1,4]\n";
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "1\n" },
    { "[1,0,0,42]\n", "42\n" },
    { "[]\n", "0\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_sparse_table_rmq_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
