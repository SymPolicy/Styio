#include "reference.hpp"

#include <numeric>
#include <string>
#include <utility>
#include <vector>

int
test_uf_component_size_cpp(
  int n,
  int x,
  const std::vector<std::pair<int, int>>& edges) {
  if (n <= 0 || x < 0 || x >= n) {
    return -1;
  }
  std::vector<int> parent(static_cast<std::size_t>(n));
  std::vector<int> sz(static_cast<std::size_t>(n), 1);
  std::iota(parent.begin(), parent.end(), 0);
  auto find = [&](int a) {
    while (parent[static_cast<std::size_t>(a)] != a) {
      parent[static_cast<std::size_t>(a)] =
        parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(a)])];
      a = parent[static_cast<std::size_t>(a)];
    }
    return a;
  };
  auto unite = [&](int a, int b) {
    a = find(a);
    b = find(b);
    if (a == b) {
      return;
    }
    if (sz[static_cast<std::size_t>(a)] < sz[static_cast<std::size_t>(b)]) {
      std::swap(a, b);
    }
    parent[static_cast<std::size_t>(b)] = a;
    sz[static_cast<std::size_t>(a)] += sz[static_cast<std::size_t>(b)];
  };
  for (const auto& [u, v] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n) {
      continue;
    }
    unite(u, v);
  }
  return sz[static_cast<std::size_t>(find(x))];
}

std::string
test_uf_component_size_cpp_output(
  int n,
  int x,
  const std::vector<std::pair<int, int>>& edges) {
  return std::to_string(test_uf_component_size_cpp(n, x, edges)) + "\n";
}
