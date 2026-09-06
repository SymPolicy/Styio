#include "reference.hpp"

#include <string>
#include <vector>

int
test_integer_partition_count_cpp(int n) {
  if (n < 0) {
    return -1;
  }
  if (n == 0) {
    return 1;
  }
  std::vector<int> dp(static_cast<std::size_t>(n) + 1, 0);
  dp[0] = 1;
  for (int coin = 1; coin <= n; ++coin) {
    for (int s = coin; s <= n; ++s) {
      dp[static_cast<std::size_t>(s)] += dp[static_cast<std::size_t>(s - coin)];
    }
  }
  return dp[static_cast<std::size_t>(n)];
}

std::string
test_integer_partition_count_cpp_output(int n) {
  return std::to_string(test_integer_partition_count_cpp(n)) + "\n";
}
