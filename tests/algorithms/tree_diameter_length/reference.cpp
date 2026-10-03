#include "reference.hpp"

#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace {

std::pair<int, int>
farthest(int n, int src, const std::vector<std::vector<int>>& g) {
  std::vector<int> dist(static_cast<std::size_t>(n), -1);
  std::queue<int> q;
  dist[static_cast<std::size_t>(src)] = 0;
  q.push(src);
  int best_v = src;
  int best_d = 0;
  while (!q.empty()) {
    const int u = q.front();
    q.pop();
    if (dist[static_cast<std::size_t>(u)] > best_d) {
      best_d = dist[static_cast<std::size_t>(u)];
      best_v = u;
    }
    for (int v : g[static_cast<std::size_t>(u)]) {
      if (dist[static_cast<std::size_t>(v)] == -1) {
        dist[static_cast<std::size_t>(v)] = dist[static_cast<std::size_t>(u)] + 1;
        q.push(v);
      }
    }
  }
  return {best_v, best_d};
}

} // namespace

int
test_tree_diameter_length_cpp(int n, const std::vector<std::pair<int, int>>& edges) {
  if (n <= 0) {
    return -1;
  }
  std::vector<std::vector<int>> g(static_cast<std::size_t>(n));
  for (const auto& [u, v] : edges) {
    if (u < 0 || v < 0 || u >= n || v >= n || u == v) {
      continue;
    }
    g[static_cast<std::size_t>(u)].push_back(v);
    g[static_cast<std::size_t>(v)].push_back(u);
  }
  std::vector<char> seen(static_cast<std::size_t>(n), 0);
  int diameter = 0;
  for (int s = 0; s < n; ++s) {
    if (seen[static_cast<std::size_t>(s)]) {
      continue;
    }
    // mark component via BFS
    std::queue<int> q;
    q.push(s);
    seen[static_cast<std::size_t>(s)] = 1;
    int any = s;
    while (!q.empty()) {
      const int u = q.front();
      q.pop();
      any = u;
      for (int v : g[static_cast<std::size_t>(u)]) {
        if (!seen[static_cast<std::size_t>(v)]) {
          seen[static_cast<std::size_t>(v)] = 1;
          q.push(v);
        }
      }
    }
    const auto [u, _] = farthest(n, any, g);
    const auto [v, d] = farthest(n, u, g);
    (void)v;
    if (d > diameter) {
      diameter = d;
    }
  }
  return diameter;
}

std::string
test_tree_diameter_length_cpp_output(int n, const std::vector<std::pair<int, int>>& edges) {
  return std::to_string(test_tree_diameter_length_cpp(n, edges)) + "\n";
}
