#include "reference.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

int
test_tree_mis_size_cpp(int n, const std::vector<std::pair<int, int>>& edges) {
  if (n <= 0) {
    return -1;
  }
  std::vector<std::vector<int>> g(static_cast<std::size_t>(n));
  for (const auto& [u, v] : edges) {
    if (u < 0 || v < 0 || u >= n || v >= n || u == v) {
      continue;
    }
    g[static_cast<std::size_t>(u)].push_back(v);
    g[static_cast<std::size_t>(v)].push_back(u);
  }
  std::vector<char> seen(static_cast<std::size_t>(n), 0);
  std::function<std::pair<int, int>(int, int)> dfs = [&](int u, int p) -> std::pair<int, int> {
    seen[static_cast<std::size_t>(u)] = 1;
    int take = 1;
    int skip = 0;
    for (int v : g[static_cast<std::size_t>(u)]) {
      if (v == p) {
        continue;
      }
      const auto [ct, cs] = dfs(v, u);
      take += cs;
      skip += ct > cs ? ct : cs;
    }
    return {take, skip};
  };
  int total = 0;
  for (int s = 0; s < n; ++s) {
    if (seen[static_cast<std::size_t>(s)]) {
      continue;
    }
    const auto [t, sk] = dfs(s, -1);
    total += t > sk ? t : sk;
  }
  return total;
}

std::string
test_tree_mis_size_cpp_output(int n, const std::vector<std::pair<int, int>>& edges) {
  return std::to_string(test_tree_mis_size_cpp(n, edges)) + "\n";
}
