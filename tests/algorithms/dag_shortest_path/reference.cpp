#include "reference.hpp"

#include <queue>
#include <string>
#include <tuple>
#include <vector>

int
test_dag_shortest_path_cpp(int n,
                           const std::vector<std::tuple<int, int, int>>& edges,
                           int s,
                           int t) {
  if (n <= 0 || s < 0 || s >= n || t < 0 || t >= n) {
    return -1;
  }
  constexpr int INF = 1000000000;
  std::vector<std::vector<std::pair<int, int>>> adj(static_cast<std::size_t>(n));
  std::vector<int> indeg(static_cast<std::size_t>(n), 0);
  for (const auto& [u, v, w] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n) {
      continue;
    }
    adj[static_cast<std::size_t>(u)].push_back({v, w});
    ++indeg[static_cast<std::size_t>(v)];
  }
  std::queue<int> q;
  for (int i = 0; i < n; ++i) {
    if (indeg[static_cast<std::size_t>(i)] == 0) {
      q.push(i);
    }
  }
  std::vector<int> order;
  order.reserve(static_cast<std::size_t>(n));
  while (!q.empty()) {
    const int u = q.front();
    q.pop();
    order.push_back(u);
    for (const auto& [v, w] : adj[static_cast<std::size_t>(u)]) {
      (void)w;
      if (--indeg[static_cast<std::size_t>(v)] == 0) {
        q.push(v);
      }
    }
  }
  if (static_cast<int>(order.size()) != n) {
    return -1; // not a DAG
  }
  std::vector<int> dist(static_cast<std::size_t>(n), INF);
  dist[static_cast<std::size_t>(s)] = 0;
  for (int u : order) {
    if (dist[static_cast<std::size_t>(u)] >= INF) {
      continue;
    }
    for (const auto& [v, w] : adj[static_cast<std::size_t>(u)]) {
      const long long cand =
        static_cast<long long>(dist[static_cast<std::size_t>(u)]) + w;
      if (cand < dist[static_cast<std::size_t>(v)]) {
        dist[static_cast<std::size_t>(v)] = static_cast<int>(cand);
      }
    }
  }
  return dist[static_cast<std::size_t>(t)];
}

std::string
test_dag_shortest_path_cpp_output(int n,
                                  const std::vector<std::tuple<int, int, int>>& edges,
                                  int s,
                                  int t) {
  return std::to_string(test_dag_shortest_path_cpp(n, edges, s, t)) + "\n";
}
