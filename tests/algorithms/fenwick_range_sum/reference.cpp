#include "reference.hpp"

#include <string>
#include <vector>

int
test_fenwick_range_sum_cpp(const std::vector<int>& a, int L, int R) {
  const int n = static_cast<int>(a.size());
  if (n <= 0 || L < 0 || R < L || R >= n) {
    return 0;
  }
  std::vector<int> bit(static_cast<std::size_t>(n + 1), 0);
  auto add = [&](int i, int delta) {
    for (++i; i <= n; i += i & -i) {
      bit[static_cast<std::size_t>(i)] += delta;
    }
  };
  auto prefix = [&](int i) {
    int sum = 0;
    for (++i; i > 0; i -= i & -i) {
      sum += bit[static_cast<std::size_t>(i)];
    }
    return sum;
  };
  for (int i = 0; i < n; ++i) {
    add(i, a[static_cast<std::size_t>(i)]);
  }
  int ans = prefix(R);
  if (L > 0) {
    ans -= prefix(L - 1);
  }
  return ans;
}

std::string
test_fenwick_range_sum_cpp_output(const std::vector<int>& a, int L, int R) {
  return std::to_string(test_fenwick_range_sum_cpp(a, L, R)) + "\n";
}
