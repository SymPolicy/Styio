#include "reference.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace {

void
dfs1(int u,
     const std::vector<std::vector<int>>& adj,
     std::vector<char>& seen,
     std::vector<int>& order) {
  seen[static_cast<std::size_t>(u)] = 1;
  for (int v : adj[static_cast<std::size_t>(u)]) {
    if (!seen[static_cast<std::size_t>(v)]) {
      dfs1(v, adj, seen, order);
    }
  }
  order.push_back(u);
}

void
dfs2(int u, const std::vector<std::vector<int>>& radj, std::vector<char>& seen) {
  seen[static_cast<std::size_t>(u)] = 1;
  for (int v : radj[static_cast<std::size_t>(u)]) {
    if (!seen[static_cast<std::size_t>(v)]) {
      dfs2(v, radj, seen);
    }
  }
}

} // namespace

int
test_scc_count_cpp(int n, const std::vector<std::pair<int, int>>& edges) {
  if (n <= 0) {
    return 0;
  }
  std::vector<std::vector<int>> adj(static_cast<std::size_t>(n));
  std::vector<std::vector<int>> radj(static_cast<std::size_t>(n));
  for (const auto& [u, v] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n) {
      continue;
    }
    adj[static_cast<std::size_t>(u)].push_back(v);
    radj[static_cast<std::size_t>(v)].push_back(u);
  }
  std::vector<char> seen(static_cast<std::size_t>(n), 0);
  std::vector<int> order;
  order.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    if (!seen[static_cast<std::size_t>(i)]) {
      dfs1(i, adj, seen, order);
    }
  }
  std::fill(seen.begin(), seen.end(), 0);
  int comps = 0;
  for (int i = n - 1; i >= 0; --i) {
    const int u = order[static_cast<std::size_t>(i)];
    if (!seen[static_cast<std::size_t>(u)]) {
      dfs2(u, radj, seen);
      ++comps;
    }
  }
  return comps;
}

std::string
test_scc_count_cpp_output(int n, const std::vector<std::pair<int, int>>& edges) {
  return std::to_string(test_scc_count_cpp(n, edges)) + "\n";
}
