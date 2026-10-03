#include "reference.hpp"

#include <string>
#include <tuple>
#include <vector>

int
test_mcmf_min_cost_cpp(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int, int>>& edges) {
  if (n <= 0 || s < 0 || t < 0 || s >= n || t >= n || s == t) {
    return -1;
  }
  const int INF = 1000000000;
  std::vector<std::vector<int>> cap(static_cast<std::size_t>(n), std::vector<int>(static_cast<std::size_t>(n), 0));
  std::vector<std::vector<int>> cost(static_cast<std::size_t>(n), std::vector<int>(static_cast<std::size_t>(n), 0));
  for (const auto& [u, v, c, w] : edges) {
    if (u < 0 || v < 0 || u >= n || v >= n || u == v || c < 0) {
      continue;
    }
    cap[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] += c;
    cost[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] = w;
    cost[static_cast<std::size_t>(v)][static_cast<std::size_t>(u)] = -w;
  }
  int flow = 0;
  int total_cost = 0;
  while (true) {
    std::vector<int> dist(static_cast<std::size_t>(n), INF);
    std::vector<int> parent(static_cast<std::size_t>(n), -1);
    dist[static_cast<std::size_t>(s)] = 0;
    bool updated = true;
    for (int it = 0; it < n && updated; ++it) {
      updated = false;
      for (int u = 0; u < n; ++u) {
        if (dist[static_cast<std::size_t>(u)] >= INF) {
          continue;
        }
        for (int v = 0; v < n; ++v) {
          if (cap[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] <= 0) {
            continue;
          }
          const int nd = dist[static_cast<std::size_t>(u)] +
                         cost[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)];
          if (nd < dist[static_cast<std::size_t>(v)]) {
            dist[static_cast<std::size_t>(v)] = nd;
            parent[static_cast<std::size_t>(v)] = u;
            updated = true;
          }
        }
      }
    }
    if (parent[static_cast<std::size_t>(t)] == -1) {
      break;
    }
    int add = INF;
    for (int v = t; v != s; v = parent[static_cast<std::size_t>(v)]) {
      const int u = parent[static_cast<std::size_t>(v)];
      const int c = cap[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)];
      if (c < add) {
        add = c;
      }
    }
    for (int v = t; v != s; v = parent[static_cast<std::size_t>(v)]) {
      const int u = parent[static_cast<std::size_t>(v)];
      cap[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] -= add;
      cap[static_cast<std::size_t>(v)][static_cast<std::size_t>(u)] += add;
    }
    total_cost += add * dist[static_cast<std::size_t>(t)];
    flow += add;
    (void)flow;
  }
  return total_cost;
}

std::string
test_mcmf_min_cost_cpp_output(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int, int>>& edges) {
  return std::to_string(test_mcmf_min_cost_cpp(n, s, t, edges)) + "\n";
}
