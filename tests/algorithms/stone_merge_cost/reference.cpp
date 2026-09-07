#include "reference.hpp"

#include <string>
#include <vector>

int
test_stone_merge_cost_cpp(const std::vector<int>& a) {
  const int n = static_cast<int>(a.size());
  if (n <= 1) {
    return 0;
  }
  std::vector<int> pref(static_cast<std::size_t>(n) + 1, 0);
  for (int i = 0; i < n; ++i) {
    pref[static_cast<std::size_t>(i) + 1] = pref[static_cast<std::size_t>(i)] + a[static_cast<std::size_t>(i)];
  }
  auto sum = [&](int l, int r) {
    return pref[static_cast<std::size_t>(r) + 1] - pref[static_cast<std::size_t>(l)];
  };
  const int INF = 1000000000;
  std::vector<std::vector<int>> dp(static_cast<std::size_t>(n), std::vector<int>(static_cast<std::size_t>(n), 0));
  for (int len = 2; len <= n; ++len) {
    for (int i = 0; i + len - 1 < n; ++i) {
      const int j = i + len - 1;
      dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = INF;
      for (int k = i; k < j; ++k) {
        const int cand = dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] +
                         dp[static_cast<std::size_t>(k + 1)][static_cast<std::size_t>(j)] +
                         sum(i, j);
        if (cand < dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]) {
          dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = cand;
        }
      }
    }
  }
  return dp[0][static_cast<std::size_t>(n) - 1];
}

std::string
test_stone_merge_cost_cpp_output(const std::vector<int>& a) {
  return std::to_string(test_stone_merge_cost_cpp(a)) + "\n";
}
