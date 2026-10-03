#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_binomial_coefficient_cpp(int n, int k) {
  if (n < 0 || k < 0 || k > n || n > 30) {
    return -1;
  }
  std::vector<int> row(static_cast<std::size_t>(k + 1), 0);
  row[0] = 1;
  for (int i = 1; i <= n; ++i) {
    for (int j = std::min(i, k); j >= 1; --j) {
      row[static_cast<std::size_t>(j)] += row[static_cast<std::size_t>(j - 1)];
    }
  }
  return row[static_cast<std::size_t>(k)];
}

std::string
test_binomial_coefficient_cpp_output(int n, int k) {
  return std::to_string(test_binomial_coefficient_cpp(n, k)) + "\n";
}
