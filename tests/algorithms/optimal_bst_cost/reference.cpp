#include "reference.hpp"

#include <string>
#include <vector>

int
test_optimal_bst_cost_cpp(const std::vector<int>& freq) {
  const int n = static_cast<int>(freq.size());
  if (n == 0) {
    return 0;
  }
  // dp[i][j] = min cost for keys i..j (0-based).
  // Convention: dp[i][i] = f[i];
  // dp[i][j] = min_r (dp[i][r-1] + dp[r+1][j]) + sum(f[i..j]).
  std::vector<std::vector<int>> dp(static_cast<std::size_t>(n),
                                   std::vector<int>(static_cast<std::size_t>(n), 0));
  std::vector<std::vector<int>> sum(static_cast<std::size_t>(n),
                                    std::vector<int>(static_cast<std::size_t>(n), 0));
  for (int i = 0; i < n; ++i) {
    sum[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] =
      freq[static_cast<std::size_t>(i)];
    dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] =
      freq[static_cast<std::size_t>(i)];
  }
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      sum[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
        sum[static_cast<std::size_t>(i)][static_cast<std::size_t>(j - 1)] +
        freq[static_cast<std::size_t>(j)];
    }
  }
  for (int len = 2; len <= n; ++len) {
    for (int i = 0; i + len - 1 < n; ++i) {
      const int j = i + len - 1;
      int best = 2000000000;
      for (int r = i; r <= j; ++r) {
        int left = 0;
        int right = 0;
        if (r > i) {
          left = dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(r - 1)];
        }
        if (r < j) {
          right = dp[static_cast<std::size_t>(r + 1)][static_cast<std::size_t>(j)];
        }
        const int cand = left + right;
        if (cand < best) {
          best = cand;
        }
      }
      dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
        best + sum[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
    }
  }
  return dp[0][static_cast<std::size_t>(n - 1)];
}

std::string
test_optimal_bst_cost_cpp_output(const std::vector<int>& freq) {
  return std::to_string(test_optimal_bst_cost_cpp(freq)) + "\n";
}
