#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_sparse_table_rmq_cpp(const std::vector<int>& a, int L, int R) {
  const int n = static_cast<int>(a.size());
  if (n <= 0 || L < 0 || R < L || R >= n) {
    return 0;
  }
  int LOG = 1;
  while ((1 << LOG) <= n) {
    ++LOG;
  }
  std::vector<std::vector<int>> st(static_cast<std::size_t>(LOG),
                                   std::vector<int>(static_cast<std::size_t>(n)));
  st[0] = a;
  for (int k = 1; k < LOG; ++k) {
    const int len = 1 << k;
    const int half = len / 2;
    for (int i = 0; i + len - 1 < n; ++i) {
      st[static_cast<std::size_t>(k)][static_cast<std::size_t>(i)] = std::min(
        st[static_cast<std::size_t>(k - 1)][static_cast<std::size_t>(i)],
        st[static_cast<std::size_t>(k - 1)][static_cast<std::size_t>(i + half)]);
    }
  }
  int k = 0;
  while ((1 << (k + 1)) <= (R - L + 1)) {
    ++k;
  }
  return std::min(
    st[static_cast<std::size_t>(k)][static_cast<std::size_t>(L)],
    st[static_cast<std::size_t>(k)][static_cast<std::size_t>(R - (1 << k) + 1)]);
}

std::string
test_sparse_table_rmq_cpp_output(const std::vector<int>& a, int L, int R) {
  return std::to_string(test_sparse_table_rmq_cpp(a, L, R)) + "\n";
}
