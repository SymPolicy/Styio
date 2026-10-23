#include "reference.hpp"

#include <queue>
#include <string>
#include <tuple>
#include <vector>

int
test_edmonds_karp_maxflow_cpp(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges) {
  if (n <= 0 || s < 0 || s >= n || t < 0 || t >= n || s == t) {
    return -1;
  }
  std::vector<std::vector<int>> cap(static_cast<std::size_t>(n),
                                    std::vector<int>(static_cast<std::size_t>(n), 0));
  for (const auto& [u, v, c] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n || c < 0 || u == v) {
      continue;
    }
    cap[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] += c;
  }
  int flow = 0;
  while (true) {
    std::vector<int> parent(static_cast<std::size_t>(n), -1);
    parent[static_cast<std::size_t>(s)] = s;
    std::queue<int> q;
    q.push(s);
    while (!q.empty() && parent[static_cast<std::size_t>(t)] == -1) {
      const int u = q.front();
      q.pop();
      for (int v = 0; v < n; ++v) {
        if (parent[static_cast<std::size_t>(v)] == -1 &&
            cap[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] > 0) {
          parent[static_cast<std::size_t>(v)] = u;
          q.push(v);
        }
      }
    }
    if (parent[static_cast<std::size_t>(t)] == -1) {
      break;
    }
    int bottleneck = 2000000000;
    for (int v = t; v != s; v = parent[static_cast<std::size_t>(v)]) {
      const int u = parent[static_cast<std::size_t>(v)];
      const int c = cap[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)];
      if (c < bottleneck) {
        bottleneck = c;
      }
    }
    for (int v = t; v != s; v = parent[static_cast<std::size_t>(v)]) {
      const int u = parent[static_cast<std::size_t>(v)];
      cap[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] -= bottleneck;
      cap[static_cast<std::size_t>(v)][static_cast<std::size_t>(u)] += bottleneck;
    }
    flow += bottleneck;
  }
  return flow;
}

std::string
test_edmonds_karp_maxflow_cpp_output(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges) {
  return std::to_string(test_edmonds_karp_maxflow_cpp(n, s, t, edges)) + "\n";
}
