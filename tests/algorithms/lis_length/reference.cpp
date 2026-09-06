#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_lis_length_cpp(const std::vector<int>& values) {
  const int n = static_cast<int>(values.size());
  if (n == 0) {
    return 0;
  }
  std::vector<int> dp(static_cast<std::size_t>(n), 1);
  int best = 1;
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < i; ++j) {
      if (values[static_cast<std::size_t>(j)] < values[static_cast<std::size_t>(i)]) {
        dp[static_cast<std::size_t>(i)] =
          std::max(dp[static_cast<std::size_t>(i)], dp[static_cast<std::size_t>(j)] + 1);
      }
    }
    best = std::max(best, dp[static_cast<std::size_t>(i)]);
  }
  return best;
}

std::string
test_lis_length_cpp_output(const std::vector<int>& values) {
  return std::to_string(test_lis_length_cpp(values)) + "\n";
}
