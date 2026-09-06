#include "reference.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

int
test_bipartite_matching_cpp(
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
  std::vector<int> match_r(static_cast<std::size_t>(nr), -1);
  std::vector<int> vis(static_cast<std::size_t>(nl), 0);
  int timer = 0;

  std::function<bool(int)> dfs = [&](int u) -> bool {
    if (vis[static_cast<std::size_t>(u)] == timer) {
      return false;
    }
    vis[static_cast<std::size_t>(u)] = timer;
    for (int v : adj[static_cast<std::size_t>(u)]) {
      const int w = match_r[static_cast<std::size_t>(v)];
      if (w == -1 || dfs(w)) {
        match_r[static_cast<std::size_t>(v)] = u;
        return true;
      }
    }
    return false;
  };

  int matching = 0;
  for (int u = 0; u < nl; ++u) {
    ++timer;
    if (dfs(u)) {
      ++matching;
    }
  }
  return matching;
}

std::string
test_bipartite_matching_cpp_output(
  int nl,
  int nr,
  const std::vector<std::pair<int, int>>& edges) {
  return std::to_string(test_bipartite_matching_cpp(nl, nr, edges)) + "\n";
}
