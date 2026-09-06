#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_rod_cutting_cpp(const std::vector<int>& prices) {
  const int n = static_cast<int>(prices.size());
  if (n <= 0) {
    return 0;
  }

  // r[j] = max revenue for length j; r[0] = 0.
  std::vector<int> r(static_cast<std::size_t>(n) + 1, 0);
  for (int j = 1; j <= n; ++j) {
    int best = prices[static_cast<std::size_t>(j - 1)];
    for (int i = 1; i < j; ++i) {
      best = std::max(best, prices[static_cast<std::size_t>(i - 1)] + r[static_cast<std::size_t>(j - i)]);
    }
    r[static_cast<std::size_t>(j)] = best;
  }
  return r[static_cast<std::size_t>(n)];
}

std::string
test_rod_cutting_cpp_output(const std::vector<int>& prices) {
  return std::to_string(test_rod_cutting_cpp(prices)) + "\n";
}
