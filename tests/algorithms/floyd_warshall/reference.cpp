#include "reference.hpp"

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

int
test_floyd_warshall_cpp(int n,
                        const std::vector<std::tuple<int, int, int>>& edges,
                        int s,
                        int t) {
  if (n <= 0 || s < 0 || s >= n || t < 0 || t >= n) {
    return -1;
  }
  constexpr int INF = 1000000000;
  std::vector<std::vector<int>> dist(static_cast<std::size_t>(n),
                                     std::vector<int>(static_cast<std::size_t>(n), INF));
  for (int i = 0; i < n; ++i) {
    dist[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] = 0;
  }
  for (const auto& [u, v, w] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n) {
      continue;
    }
    dist[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] =
      std::min(dist[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)], w);
  }
  for (int k = 0; k < n; ++k) {
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        if (dist[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] >= INF ||
            dist[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)] >= INF) {
          continue;
        }
        const long long cand =
          static_cast<long long>(dist[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)]) +
          dist[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)];
        if (cand < dist[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]) {
          dist[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
            static_cast<int>(cand);
        }
      }
    }
  }
  return dist[static_cast<std::size_t>(s)][static_cast<std::size_t>(t)];
}

std::string
test_floyd_warshall_cpp_output(int n,
                               const std::vector<std::tuple<int, int, int>>& edges,
                               int s,
                               int t) {
  return std::to_string(test_floyd_warshall_cpp(n, edges, s, t)) + "\n";
}
