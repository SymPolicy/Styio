#include "reference.hpp"

#include <string>
#include <tuple>
#include <vector>

int
test_bellman_ford_cpp(int n,
                      const std::vector<std::tuple<int, int, int>>& edges,
                      int s,
                      int t) {
  if (n <= 0 || s < 0 || s >= n || t < 0 || t >= n) {
    return -1;
  }
  constexpr int INF = 1000000000;
  std::vector<int> dist(static_cast<std::size_t>(n), INF);
  dist[static_cast<std::size_t>(s)] = 0;

  for (int iter = 0; iter < n - 1; ++iter) {
    bool changed = false;
    for (const auto& [u, v, w] : edges) {
      if (u < 0 || u >= n || v < 0 || v >= n) {
        continue;
      }
      if (dist[static_cast<std::size_t>(u)] >= INF) {
        continue;
      }
      const long long cand =
        static_cast<long long>(dist[static_cast<std::size_t>(u)]) + w;
      if (cand < dist[static_cast<std::size_t>(v)]) {
        dist[static_cast<std::size_t>(v)] = static_cast<int>(cand);
        changed = true;
      }
    }
    if (!changed) {
      break;
    }
  }

  for (const auto& [u, v, w] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n) {
      continue;
    }
    if (dist[static_cast<std::size_t>(u)] >= INF) {
      continue;
    }
    const long long cand =
      static_cast<long long>(dist[static_cast<std::size_t>(u)]) + w;
    if (cand < dist[static_cast<std::size_t>(v)]) {
      return -1; // negative cycle
    }
  }

  return dist[static_cast<std::size_t>(t)];
}

std::string
test_bellman_ford_cpp_output(int n,
                             const std::vector<std::tuple<int, int, int>>& edges,
                             int s,
                             int t) {
  return std::to_string(test_bellman_ford_cpp(n, edges, s, t)) + "\n";
}
