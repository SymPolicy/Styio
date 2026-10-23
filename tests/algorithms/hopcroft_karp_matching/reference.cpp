#include "reference.hpp"

#include <functional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

int
test_hopcroft_karp_matching_cpp(
  int nl,
  int nr,
  const std::vector<std::pair<int, int>>& edges) {
  if (nl <= 0 || nr <= 0) {
    return 0;
  }
  std::vector<std::vector<int>> adj(static_cast<std::size_t>(nl));
  for (const auto& [u, v] : edges) {
    if (u < 0 || u >= nl || v < 0 || v >= nr) {
      continue;
    }
    adj[static_cast<std::size_t>(u)].push_back(v);
  }
  const int INF = 1000000000;
  std::vector<int> pair_u(static_cast<std::size_t>(nl), -1);
  std::vector<int> pair_v(static_cast<std::size_t>(nr), -1);
  std::vector<int> dist(static_cast<std::size_t>(nl));

  auto bfs = [&]() -> bool {
    std::queue<int> q;
    for (int u = 0; u < nl; ++u) {
      if (pair_u[static_cast<std::size_t>(u)] == -1) {
        dist[static_cast<std::size_t>(u)] = 0;
        q.push(u);
      } else {
        dist[static_cast<std::size_t>(u)] = INF;
      }
    }
    int found = INF;
    while (!q.empty()) {
      const int u = q.front();
      q.pop();
      if (dist[static_cast<std::size_t>(u)] >= found) {
        continue;
      }
      for (int v : adj[static_cast<std::size_t>(u)]) {
        const int u2 = pair_v[static_cast<std::size_t>(v)];
        if (u2 == -1) {
          found = dist[static_cast<std::size_t>(u)] + 1;
        } else if (dist[static_cast<std::size_t>(u2)] == INF) {
          dist[static_cast<std::size_t>(u2)] = dist[static_cast<std::size_t>(u)] + 1;
          q.push(u2);
        }
      }
    }
    return found != INF;
  };

  std::function<bool(int)> dfs = [&](int u) -> bool {
    for (int v : adj[static_cast<std::size_t>(u)]) {
      const int u2 = pair_v[static_cast<std::size_t>(v)];
      if (u2 == -1 ||
          (dist[static_cast<std::size_t>(u2)] ==
             dist[static_cast<std::size_t>(u)] + 1 &&
           dfs(u2))) {
        pair_u[static_cast<std::size_t>(u)] = v;
        pair_v[static_cast<std::size_t>(v)] = u;
        return true;
      }
    }
    dist[static_cast<std::size_t>(u)] = INF;
    return false;
  };

  // include functional
  int matching = 0;
  while (bfs()) {
    for (int u = 0; u < nl; ++u) {
      if (pair_u[static_cast<std::size_t>(u)] == -1 && dfs(u)) {
        ++matching;
      }
    }
  }
  return matching;
}

std::string
test_hopcroft_karp_matching_cpp_output(
  int nl,
  int nr,
  const std::vector<std::pair<int, int>>& edges) {
  return std::to_string(test_hopcroft_karp_matching_cpp(nl, nr, edges)) + "\n";
}
