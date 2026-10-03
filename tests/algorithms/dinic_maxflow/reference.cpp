#include "reference.hpp"

#include <algorithm>
#include <functional>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

int
test_dinic_maxflow_cpp(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges) {
  if (n <= 0 || s < 0 || s >= n || t < 0 || t >= n || s == t) {
    return -1;
  }
  struct Edge {
    int to = 0;
    int rev = 0;
    int cap = 0;
  };
  std::vector<std::vector<Edge>> g(static_cast<std::size_t>(n));
  auto add_edge = [&](int u, int v, int c) {
    Edge a{v, static_cast<int>(g[static_cast<std::size_t>(v)].size()), c};
    Edge b{u, static_cast<int>(g[static_cast<std::size_t>(u)].size()), 0};
    g[static_cast<std::size_t>(u)].push_back(a);
    g[static_cast<std::size_t>(v)].push_back(b);
  };
  for (const auto& [u, v, c] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n || c < 0 || u == v) {
      continue;
    }
    add_edge(u, v, c);
  }
  std::vector<int> level(static_cast<std::size_t>(n));
  std::vector<int> it(static_cast<std::size_t>(n));
  auto bfs = [&]() -> bool {
    std::fill(level.begin(), level.end(), -1);
    level[static_cast<std::size_t>(s)] = 0;
    std::queue<int> q;
    q.push(s);
    while (!q.empty()) {
      const int u = q.front();
      q.pop();
      for (const Edge& e : g[static_cast<std::size_t>(u)]) {
        if (e.cap > 0 && level[static_cast<std::size_t>(e.to)] < 0) {
          level[static_cast<std::size_t>(e.to)] = level[static_cast<std::size_t>(u)] + 1;
          q.push(e.to);
        }
      }
    }
    return level[static_cast<std::size_t>(t)] >= 0;
  };
  std::function<int(int, int)> dfs = [&](int u, int f) -> int {
    if (u == t) {
      return f;
    }
    for (int& i = it[static_cast<std::size_t>(u)];
         i < static_cast<int>(g[static_cast<std::size_t>(u)].size()); ++i) {
      Edge& e = g[static_cast<std::size_t>(u)][static_cast<std::size_t>(i)];
      if (e.cap <= 0 || level[static_cast<std::size_t>(u)] + 1 !=
                          level[static_cast<std::size_t>(e.to)]) {
        continue;
      }
      const int pushed = dfs(e.to, std::min(f, e.cap));
      if (pushed > 0) {
        e.cap -= pushed;
        g[static_cast<std::size_t>(e.to)][static_cast<std::size_t>(e.rev)].cap += pushed;
        return pushed;
      }
    }
    return 0;
  };
  // Need <functional> for std::function — include it
  int flow = 0;
  while (bfs()) {
    std::fill(it.begin(), it.end(), 0);
    while (true) {
      const int pushed = dfs(s, 2000000000);
      if (pushed == 0) {
        break;
      }
      flow += pushed;
    }
  }
  return flow;
}

std::string
test_dinic_maxflow_cpp_output(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges) {
  return std::to_string(test_dinic_maxflow_cpp(n, s, t, edges)) + "\n";
}
