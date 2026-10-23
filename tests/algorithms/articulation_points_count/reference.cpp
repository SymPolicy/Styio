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

// Components among vertices that are not equal to `skip` (-1 = none).
int
components_skip_vertex(
  int n, const std::vector<std::pair<int, int>>& edges, int skip) {
  std::vector<int> parent(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    parent[static_cast<std::size_t>(i)] = i;
  }
  for (const auto& [u, v] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n || u == v) {
      continue;
    }
    if (u == skip || v == skip) {
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
    if (i == skip) {
      continue;
    }
    if (find_root(parent, i) == i) {
      ++comps;
    }
  }
  return comps;
}

} // namespace

int
test_articulation_points_count_cpp(
  int n, const std::vector<std::pair<int, int>>& edges) {
  if (n <= 1) {
    return 0;
  }
  const int base = components_skip_vertex(n, edges, -1);
  int arts = 0;
  for (int v = 0; v < n; ++v) {
    if (components_skip_vertex(n, edges, v) > base) {
      ++arts;
    }
  }
  return arts;
}

std::string
test_articulation_points_count_cpp_output(
  int n, const std::vector<std::pair<int, int>>& edges) {
  return std::to_string(test_articulation_points_count_cpp(n, edges)) + "\n";
}
