#include "reference.hpp"

#include <string>
#include <vector>

int
test_catalan_number_cpp(int n) {
  if (n < 0) {
    return 0;
  }
  if (n <= 1) {
    return 1;
  }
  std::vector<long long> C(static_cast<std::size_t>(n) + 1, 0);
  C[0] = 1;
  for (int i = 1; i <= n; ++i) {
    long long sum = 0;
    for (int j = 0; j < i; ++j) {
      sum += C[static_cast<std::size_t>(j)] * C[static_cast<std::size_t>(i - 1 - j)];
    }
    C[static_cast<std::size_t>(i)] = sum;
  }
  return static_cast<int>(C[static_cast<std::size_t>(n)]);
}

std::string
test_catalan_number_cpp_output(int n) {
  return std::to_string(test_catalan_number_cpp(n)) + "\n";
}
