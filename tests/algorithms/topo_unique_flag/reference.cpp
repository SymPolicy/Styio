#include "reference.hpp"

#include <queue>
#include <string>
#include <utility>
#include <vector>

int
test_topo_unique_flag_cpp(int n, const std::vector<std::pair<int, int>>& edges) {
  if (n <= 0) {
    return 0;
  }
  std::vector<std::vector<int>> g(static_cast<std::size_t>(n));
  std::vector<int> indeg(static_cast<std::size_t>(n), 0);
  for (const auto& [u, v] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n) {
      return 0;
    }
    g[static_cast<std::size_t>(u)].push_back(v);
    ++indeg[static_cast<std::size_t>(v)];
  }
  std::queue<int> q;
  for (int i = 0; i < n; ++i) {
    if (indeg[static_cast<std::size_t>(i)] == 0) {
      q.push(i);
    }
  }
  int seen = 0;
  while (!q.empty()) {
    if (q.size() > 1) {
      return 0; // branching choice => non-unique
    }
    const int u = q.front();
    q.pop();
    ++seen;
    for (int v : g[static_cast<std::size_t>(u)]) {
      if (--indeg[static_cast<std::size_t>(v)] == 0) {
        q.push(v);
      }
    }
  }
  return seen == n ? 1 : 0;
}

std::string
test_topo_unique_flag_cpp_output(int n, const std::vector<std::pair<int, int>>& edges) {
  return std::to_string(test_topo_unique_flag_cpp(n, edges)) + "\n";
}
