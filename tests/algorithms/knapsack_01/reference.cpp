#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_knapsack_01_cpp(int capacity,
                     const std::vector<int>& weights,
                     const std::vector<int>& values) {
  if (capacity < 0 || weights.size() != values.size()) {
    return 0;
  }
  const int n = static_cast<int>(weights.size());
  std::vector<int> dp(static_cast<std::size_t>(capacity) + 1, 0);
  for (int i = 0; i < n; ++i) {
    const int w = weights[static_cast<std::size_t>(i)];
    const int v = values[static_cast<std::size_t>(i)];
    if (w <= 0) {
      continue;
    }
    for (int c = capacity; c >= w; --c) {
      dp[static_cast<std::size_t>(c)] =
        std::max(dp[static_cast<std::size_t>(c)], dp[static_cast<std::size_t>(c - w)] + v);
    }
  }
  return dp[static_cast<std::size_t>(capacity)];
}

std::string
test_knapsack_01_cpp_output(int capacity,
                            const std::vector<int>& weights,
                            const std::vector<int>& values) {
  return std::to_string(test_knapsack_01_cpp(capacity, weights, values)) + "\n";
}
