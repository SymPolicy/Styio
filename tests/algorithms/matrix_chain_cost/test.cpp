#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<int>
test_matrix_chain_cost_random_dims(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 8);
  std::uniform_int_distribution<int> d_dist(1, 12);
  const int n = n_dist(rng);
  std::vector<int> dims(static_cast<std::size_t>(n) + 1);
  for (int& d : dims) {
    d = d_dist(rng);
  }
  return dims;
}

std::filesystem::path
test_matrix_chain_cost_styio() {
  return styio::testing::algorithms::styio_program("matrix_chain_cost", "matrix_chain_cost.styio");
}

std::string
format_matrix_chain_cost_input(const std::vector<int>& dims) {
  const int p = static_cast<int>(dims.size()) - 1;
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(1 + dims.size() + (p + 1) * (p + 1)));
  encoded.push_back(p);
  encoded.insert(encoded.end(), dims.begin(), dims.end());
  for (int i = 0; i < (p + 1) * (p + 1); ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_matrix_chain_cost) {
  std::mt19937 rng(0xC1512);

  for (int iteration = 0; iteration < 120; ++iteration) {
    const std::vector<int> dims = test_matrix_chain_cost_random_dims(rng);
    const std::string expected = test_matrix_chain_cost_cpp_output(dims);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_matrix_chain_cost_styio(), format_matrix_chain_cost_input(dims));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_matrix_chain_cost_input(dims)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_matrix_chain_cost_fixed_cases) {
  // CLRS example: dims 30,35,15,5,10,20,25 -> 15125; p=6; workspace 7*7=49
  std::string clrs = "[6,30,35,15,5,10,20,25";
  for (int i = 0; i < 49; ++i) {
    clrs += ",0";
  }
  clrs += "]\n";

  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[1,2,3,0,0,0,0]\n", "0\n" },
    { clrs, "15125\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_matrix_chain_cost_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
