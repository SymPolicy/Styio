#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_coin_change_min_cpp(int amount, const std::vector<int>& coins) {
  if (amount < 0) {
    return -1;
  }
  if (amount == 0) {
    return 0;
  }
  constexpr int INF = 1000000000;
  std::vector<int> dp(static_cast<std::size_t>(amount) + 1, INF);
  dp[0] = 0;
  for (int a = 1; a <= amount; ++a) {
    for (int c : coins) {
      if (c <= 0 || c > a) {
        continue;
      }
      if (dp[static_cast<std::size_t>(a - c)] < INF) {
        dp[static_cast<std::size_t>(a)] =
          std::min(dp[static_cast<std::size_t>(a)],
                   dp[static_cast<std::size_t>(a - c)] + 1);
      }
    }
  }
  return dp[static_cast<std::size_t>(amount)] >= INF ? -1
                                                     : dp[static_cast<std::size_t>(amount)];
}

std::string
test_coin_change_min_cpp_output(int amount, const std::vector<int>& coins) {
  return std::to_string(test_coin_change_min_cpp(amount, coins)) + "\n";
}
