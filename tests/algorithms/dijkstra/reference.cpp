#include "reference.hpp"

#include <queue>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

int
test_dijkstra_cpp(int n,
                  const std::vector<std::tuple<int, int, int>>& edges,
                  int s,
                  int t) {
  if (n <= 0 || s < 0 || s >= n || t < 0 || t >= n) {
    return -1;
  }
  constexpr int INF = 1000000000;
  std::vector<std::vector<std::pair<int, int>>> adj(static_cast<std::size_t>(n));
  for (const auto& [u, v, w] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n || w < 0) {
      continue;
    }
    adj[static_cast<std::size_t>(u)].push_back({v, w});
  }

  std::vector<int> dist(static_cast<std::size_t>(n), INF);
  using Node = std::pair<int, int>; // dist, vertex
  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
  dist[static_cast<std::size_t>(s)] = 0;
  pq.push({0, s});
  while (!pq.empty()) {
    const auto [du, u] = pq.top();
    pq.pop();
    if (du != dist[static_cast<std::size_t>(u)]) {
      continue;
    }
    if (u == t) {
      break;
    }
    for (const auto& [v, w] : adj[static_cast<std::size_t>(u)]) {
      const int cand = du + w;
      if (cand < dist[static_cast<std::size_t>(v)]) {
        dist[static_cast<std::size_t>(v)] = cand;
        pq.push({cand, v});
      }
    }
  }
  return dist[static_cast<std::size_t>(t)];
}

std::string
test_dijkstra_cpp_output(int n,
                         const std::vector<std::tuple<int, int, int>>& edges,
                         int s,
                         int t) {
  return std::to_string(test_dijkstra_cpp(n, edges, s, t)) + "\n";
}
