#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_lca_binary_lifting_cpp(const std::vector<int>& parent, int u, int v) {
  const int n = static_cast<int>(parent.size());
  if (n <= 0 || u < 0 || v < 0 || u >= n || v >= n) {
    return -1;
  }
  if (parent[0] != -1) {
    return -1;
  }
  for (int i = 1; i < n; ++i) {
    const int p = parent[static_cast<std::size_t>(i)];
    if (p < 0 || p >= n || p == i) {
      return -1;
    }
  }
  int LOG = 1;
  while ((1 << LOG) < n) {
    ++LOG;
  }
  std::vector<std::vector<int>> up(static_cast<std::size_t>(LOG),
                                   std::vector<int>(static_cast<std::size_t>(n), -1));
  std::vector<int> depth(static_cast<std::size_t>(n), 0);
  up[0] = parent;
  up[0][0] = 0;
  for (int k = 1; k < LOG; ++k) {
    for (int i = 0; i < n; ++i) {
      const int mid = up[static_cast<std::size_t>(k - 1)][static_cast<std::size_t>(i)];
      up[static_cast<std::size_t>(k)][static_cast<std::size_t>(i)] =
        up[static_cast<std::size_t>(k - 1)][static_cast<std::size_t>(mid)];
    }
  }
  // depths via parent climb (tree assumed)
  for (int i = 1; i < n; ++i) {
    int d = 0;
    int x = i;
    while (x != 0) {
      x = parent[static_cast<std::size_t>(x)];
      ++d;
      if (d > n) {
        return -1;
      }
    }
    depth[static_cast<std::size_t>(i)] = d;
  }
  auto lift = [&](int x, int diff) {
    for (int k = 0; k < LOG; ++k) {
      if ((diff >> k) & 1) {
        x = up[static_cast<std::size_t>(k)][static_cast<std::size_t>(x)];
      }
    }
    return x;
  };
  if (depth[static_cast<std::size_t>(u)] < depth[static_cast<std::size_t>(v)]) {
    std::swap(u, v);
  }
  u = lift(u, depth[static_cast<std::size_t>(u)] - depth[static_cast<std::size_t>(v)]);
  if (u == v) {
    return u;
  }
  for (int k = LOG - 1; k >= 0; --k) {
    if (up[static_cast<std::size_t>(k)][static_cast<std::size_t>(u)] !=
        up[static_cast<std::size_t>(k)][static_cast<std::size_t>(v)]) {
      u = up[static_cast<std::size_t>(k)][static_cast<std::size_t>(u)];
      v = up[static_cast<std::size_t>(k)][static_cast<std::size_t>(v)];
    }
  }
  return parent[static_cast<std::size_t>(u)];
}

std::string
test_lca_binary_lifting_cpp_output(const std::vector<int>& parent, int u, int v) {
  return std::to_string(test_lca_binary_lifting_cpp(parent, u, v)) + "\n";
}
