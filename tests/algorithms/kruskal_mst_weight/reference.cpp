#include "reference.hpp"

#include <algorithm>
#include <numeric>
#include <string>
#include <tuple>
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

} // namespace

int
test_kruskal_mst_weight_cpp(int n, std::vector<std::tuple<int, int, int>> edges) {
  if (n <= 0) {
    return 0;
  }
  std::sort(edges.begin(), edges.end(),
            [](const auto& a, const auto& b) { return std::get<2>(a) < std::get<2>(b); });
  std::vector<int> parent(static_cast<std::size_t>(n));
  std::iota(parent.begin(), parent.end(), 0);
  int total = 0;
  int used = 0;
  for (const auto& [u, v, w] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n || u == v) {
      continue;
    }
    const int ru = find_root(parent, u);
    const int rv = find_root(parent, v);
    if (ru == rv) {
      continue;
    }
    parent[static_cast<std::size_t>(ru)] = rv;
    total += w;
    ++used;
    if (used == n - 1) {
      break;
    }
  }
  return total;
}

std::string
test_kruskal_mst_weight_cpp_output(int n, const std::vector<std::tuple<int, int, int>>& edges) {
  return std::to_string(test_kruskal_mst_weight_cpp(n, edges)) + "\n";
}
