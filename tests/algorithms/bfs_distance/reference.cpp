#include "reference.hpp"

#include <queue>
#include <vector>

int
test_bfs_distance_cpp(int n,
                      const std::vector<std::pair<int, int>>& edges,
                      int s,
                      int t) {
  if (n <= 0 || s < 0 || s >= n || t < 0 || t >= n) {
    return -1;
  }
  if (s == t) {
    return 0;
  }

  std::vector<std::vector<int>> adj(static_cast<std::size_t>(n));
  for (const auto& [u, v] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n) {
      continue;
    }
    adj[static_cast<std::size_t>(u)].push_back(v);
  }

  std::vector<int> dist(static_cast<std::size_t>(n), -1);
  std::queue<int> q;
  dist[static_cast<std::size_t>(s)] = 0;
  q.push(s);
  while (!q.empty()) {
    const int u = q.front();
    q.pop();
    if (u == t) {
      return dist[static_cast<std::size_t>(u)];
    }
    for (int v : adj[static_cast<std::size_t>(u)]) {
      if (dist[static_cast<std::size_t>(v)] < 0) {
        dist[static_cast<std::size_t>(v)] = dist[static_cast<std::size_t>(u)] + 1;
        q.push(v);
      }
    }
  }
  return dist[static_cast<std::size_t>(t)];
}

std::string
test_bfs_distance_cpp_output(int n,
                             const std::vector<std::pair<int, int>>& edges,
                             int s,
                             int t) {
  return std::to_string(test_bfs_distance_cpp(n, edges, s, t)) + "\n";
}
