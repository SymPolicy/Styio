#include "reference.hpp"

#include <string>
#include <vector>

int
test_segment_tree_range_sum_cpp(const std::vector<int>& a, int L, int R) {
  const int n = static_cast<int>(a.size());
  if (n <= 0 || L < 0 || R < L || R >= n) {
    return 0;
  }
  std::vector<int> tree(static_cast<std::size_t>(4 * n + 4), 0);
  auto build = [&](auto&& self, int node, int l, int r) -> void {
    if (l == r) {
      tree[static_cast<std::size_t>(node)] = a[static_cast<std::size_t>(l)];
      return;
    }
    const int mid = (l + r) / 2;
    self(self, node * 2, l, mid);
    self(self, node * 2 + 1, mid + 1, r);
    tree[static_cast<std::size_t>(node)] =
      tree[static_cast<std::size_t>(node * 2)] + tree[static_cast<std::size_t>(node * 2 + 1)];
  };
  auto query = [&](auto&& self, int node, int l, int r, int ql, int qr) -> int {
    if (qr < l || r < ql) {
      return 0;
    }
    if (ql <= l && r <= qr) {
      return tree[static_cast<std::size_t>(node)];
    }
    const int mid = (l + r) / 2;
    return self(self, node * 2, l, mid, ql, qr) +
           self(self, node * 2 + 1, mid + 1, r, ql, qr);
  };
  build(build, 1, 0, n - 1);
  return query(query, 1, 0, n - 1, L, R);
}

std::string
test_segment_tree_range_sum_cpp_output(const std::vector<int>& a, int L, int R) {
  return std::to_string(test_segment_tree_range_sum_cpp(a, L, R)) + "\n";
}
