#include "reference.hpp"

#include <string>
#include <utility>
#include <vector>

namespace {

int
find_root(std::vector<int>& parent, int x) {
  while (parent[static_cast<std::size_t>(x)] != x) {
    parent[static_cast<std::size_t>(x)] =
      parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])];
    x = parent[static_cast<std::size_t>(x)];
  }
  return x;
}

int
components(int n, const std::vector<std::pair<int, int>>& edges, int skip) {
  std::vector<int> parent(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    parent[static_cast<std::size_t>(i)] = i;
  }
  for (int i = 0; i < static_cast<int>(edges.size()); ++i) {
    if (i == skip) {
      continue;
    }
    const auto [u, v] = edges[static_cast<std::size_t>(i)];
    if (u < 0 || u >= n || v < 0 || v >= n || u == v) {
      continue;
    }
    const int ru = find_root(parent, u);
    const int rv = find_root(parent, v);
    if (ru != rv) {
      parent[static_cast<std::size_t>(ru)] = rv;
    }
  }
  int comps = 0;
  for (int i = 0; i < n; ++i) {
    if (find_root(parent, i) == i) {
      ++comps;
    }
  }
  return comps;
}

} // namespace

int
test_bridges_count_cpp(int n, const std::vector<std::pair<int, int>>& edges) {
  if (n <= 0) {
    return 0;
  }
  const int base = components(n, edges, -1);
  int bridges = 0;
  for (int i = 0; i < static_cast<int>(edges.size()); ++i) {
    const auto [u, v] = edges[static_cast<std::size_t>(i)];
    if (u < 0 || u >= n || v < 0 || v >= n || u == v) {
      continue;
    }
    if (components(n, edges, i) > base) {
      ++bridges;
    }
  }
  return bridges;
}

std::string
test_bridges_count_cpp_output(int n, const std::vector<std::pair<int, int>>& edges) {
  return std::to_string(test_bridges_count_cpp(n, edges)) + "\n";
}
