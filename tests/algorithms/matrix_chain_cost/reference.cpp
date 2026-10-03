#include "reference.hpp"

#include <limits>
#include <string>
#include <vector>

int
test_matrix_chain_cost_cpp(const std::vector<int>& dims) {
  const int n = static_cast<int>(dims.size()) - 1;
  if (n < 1) {
    return 0;
  }
  std::vector<std::vector<long long>> m(
    static_cast<std::size_t>(n) + 1,
    std::vector<long long>(static_cast<std::size_t>(n) + 1, 0));
  for (int len = 2; len <= n; ++len) {
    for (int i = 1; i <= n - len + 1; ++i) {
      const int j = i + len - 1;
      m[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
        std::numeric_limits<long long>::max() / 4;
      for (int k = i; k < j; ++k) {
        const long long cost =
          m[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] +
          m[static_cast<std::size_t>(k + 1)][static_cast<std::size_t>(j)] +
          static_cast<long long>(dims[static_cast<std::size_t>(i - 1)]) *
            dims[static_cast<std::size_t>(k)] *
            dims[static_cast<std::size_t>(j)];
        if (cost < m[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]) {
          m[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = cost;
        }
      }
    }
  }
  return static_cast<int>(m[1][static_cast<std::size_t>(n)]);
}

std::string
test_matrix_chain_cost_cpp_output(const std::vector<int>& dims) {
  return std::to_string(test_matrix_chain_cost_cpp(dims)) + "\n";
}
