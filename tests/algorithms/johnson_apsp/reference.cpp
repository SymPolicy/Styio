#include "reference.hpp"

#include <limits>
#include <queue>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

constexpr int kInf = 1000000000;

} // namespace

int
test_johnson_apsp_cpp(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges) {
  if (n <= 0 || s < 0 || s >= n || t < 0 || t >= n) {
    return -1;
  }
  // Bellman-Ford from dummy vertex  n  connected to all with weight 0.
  std::vector<int> h(static_cast<std::size_t>(n + 1), 0);
  std::vector<std::tuple<int, int, int>> all = edges;
  for (int v = 0; v < n; ++v) {
    all.emplace_back(n, v, 0);
  }
  for (int iter = 0; iter < n; ++iter) {
    bool updated = false;
    for (const auto& [u, v, w] : all) {
      if (u < 0 || u > n || v < 0 || v > n) {
        continue;
      }
      if (h[static_cast<std::size_t>(u)] >= kInf) {
        continue;
      }
      const long long cand =
        static_cast<long long>(h[static_cast<std::size_t>(u)]) + w;
      if (cand < h[static_cast<std::size_t>(v)]) {
        h[static_cast<std::size_t>(v)] = static_cast<int>(cand);
        updated = true;
      }
    }
    if (!updated) {
      break;
    }
  }
  for (const auto& [u, v, w] : all) {
    if (u < 0 || u > n || v < 0 || v > n) {
      continue;
    }
    if (h[static_cast<std::size_t>(u)] >= kInf) {
      continue;
    }
    const long long cand =
      static_cast<long long>(h[static_cast<std::size_t>(u)]) + w;
    if (cand < h[static_cast<std::size_t>(v)]) {
      return -1; // negative cycle
    }
  }

  std::vector<std::vector<std::pair<int, int>>> adj(static_cast<std::size_t>(n));
  for (const auto& [u, v, w] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n) {
      continue;
    }
    const int ww = w + h[static_cast<std::size_t>(u)] - h[static_cast<std::size_t>(v)];
    adj[static_cast<std::size_t>(u)].push_back({v, ww});
  }

  std::vector<int> dist(static_cast<std::size_t>(n), kInf);
  using Node = std::pair<int, int>;
  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
  dist[static_cast<std::size_t>(s)] = 0;
  pq.push({0, s});
  while (!pq.empty()) {
    const auto [d, u] = pq.top();
    pq.pop();
    if (d != dist[static_cast<std::size_t>(u)]) {
      continue;
    }
    for (const auto& [v, w] : adj[static_cast<std::size_t>(u)]) {
      const long long nd = static_cast<long long>(d) + w;
      if (nd < dist[static_cast<std::size_t>(v)]) {
        dist[static_cast<std::size_t>(v)] = static_cast<int>(nd);
        pq.push({dist[static_cast<std::size_t>(v)], v});
      }
    }
  }
  if (dist[static_cast<std::size_t>(t)] >= kInf) {
    return kInf;
  }
  return dist[static_cast<std::size_t>(t)] + h[static_cast<std::size_t>(t)] -
         h[static_cast<std::size_t>(s)];
}

std::string
test_johnson_apsp_cpp_output(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges) {
  return std::to_string(test_johnson_apsp_cpp(n, s, t, edges)) + "\n";
}
