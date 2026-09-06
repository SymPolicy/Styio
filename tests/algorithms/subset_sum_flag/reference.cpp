#include "reference.hpp"

#include <string>
#include <vector>

int
test_subset_sum_flag_cpp(int target, const std::vector<int>& values) {
  if (target < 0) {
    return 0;
  }
  if (target == 0) {
    return 1;
  }
  std::vector<char> dp(static_cast<std::size_t>(target) + 1, 0);
  dp[0] = 1;
  for (int v : values) {
    if (v <= 0) {
      continue;
    }
    for (int s = target; s >= v; --s) {
      if (dp[static_cast<std::size_t>(s - v)]) {
        dp[static_cast<std::size_t>(s)] = 1;
      }
    }
  }
  return dp[static_cast<std::size_t>(target)] ? 1 : 0;
}

std::string
test_subset_sum_flag_cpp_output(int target, const std::vector<int>& values) {
  return std::to_string(test_subset_sum_flag_cpp(target, values)) + "\n";
}
