#include "reference.hpp"

#include <string>
#include <tuple>
#include <vector>

int
test_prim_mst_weight_cpp(int n, std::vector<std::tuple<int, int, int>> edges) {
  if (n <= 0) {
    return 0;
  }
  constexpr int INF = 1000000000;
  std::vector<std::vector<int>> adj(static_cast<std::size_t>(n),
                                    std::vector<int>(static_cast<std::size_t>(n), INF));
  for (int i = 0; i < n; ++i) {
    adj[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] = 0;
  }
  for (const auto& [u, v, w] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n || u == v) {
      continue;
    }
    if (w < adj[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)]) {
      adj[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] = w;
      adj[static_cast<std::size_t>(v)][static_cast<std::size_t>(u)] = w;
    }
  }

  std::vector<char> in_mst(static_cast<std::size_t>(n), 0);
  int total = 0;
  int remaining = n;
  while (remaining > 0) {
    int start = -1;
    for (int i = 0; i < n; ++i) {
      if (!in_mst[static_cast<std::size_t>(i)]) {
        start = i;
        break;
      }
    }
    if (start < 0) {
      break;
    }
    std::vector<int> key(static_cast<std::size_t>(n), INF);
    key[static_cast<std::size_t>(start)] = 0;
    const int budget = remaining;
    for (int iter = 0; iter < budget; ++iter) {
      int u = -1;
      int best = INF;
      for (int i = 0; i < n; ++i) {
        if (!in_mst[static_cast<std::size_t>(i)] && key[static_cast<std::size_t>(i)] < best) {
          best = key[static_cast<std::size_t>(i)];
          u = i;
        }
      }
      if (u < 0 || best >= INF) {
        break;
      }
      in_mst[static_cast<std::size_t>(u)] = 1;
      --remaining;
      if (best > 0) {
        total += best;
      }
      for (int v = 0; v < n; ++v) {
        const int w = adj[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)];
        if (!in_mst[static_cast<std::size_t>(v)] && w < key[static_cast<std::size_t>(v)]) {
          key[static_cast<std::size_t>(v)] = w;
        }
      }
    }
  }
  return total;
}

std::string
test_prim_mst_weight_cpp_output(int n, const std::vector<std::tuple<int, int, int>>& edges) {
  return std::to_string(test_prim_mst_weight_cpp(n, edges)) + "\n";
}
