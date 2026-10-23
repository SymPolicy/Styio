#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_edit_distance_cpp(const std::vector<int>& a, const std::vector<int>& b) {
  const int n = static_cast<int>(a.size());
  const int m = static_cast<int>(b.size());
  std::vector<std::vector<int>> dp(static_cast<std::size_t>(n) + 1,
                                   std::vector<int>(static_cast<std::size_t>(m) + 1, 0));
  for (int i = 0; i <= n; ++i) {
    dp[static_cast<std::size_t>(i)][0] = i;
  }
  for (int j = 0; j <= m; ++j) {
    dp[0][static_cast<std::size_t>(j)] = j;
  }
  for (int i = 1; i <= n; ++i) {
    for (int j = 1; j <= m; ++j) {
      const int cost = a[static_cast<std::size_t>(i - 1)] == b[static_cast<std::size_t>(j - 1)] ? 0 : 1;
      dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = std::min({
        dp[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j)] + 1,
        dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(j - 1)] + 1,
        dp[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j - 1)] + cost,
      });
    }
  }
  return dp[static_cast<std::size_t>(n)][static_cast<std::size_t>(m)];
}

std::string
test_edit_distance_cpp_output(const std::vector<int>& a, const std::vector<int>& b) {
  return std::to_string(test_edit_distance_cpp(a, b)) + "\n";
}
