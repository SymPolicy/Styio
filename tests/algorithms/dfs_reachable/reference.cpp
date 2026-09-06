#include "reference.hpp"

#include <stack>
#include <vector>

int
test_dfs_reachable_cpp(int n,
                       const std::vector<std::pair<int, int>>& edges,
                       int s,
                       int t) {
  if (n <= 0 || s < 0 || s >= n || t < 0 || t >= n) {
    return 0;
  }
  if (s == t) {
    return 1;
  }

  std::vector<std::vector<int>> adj(static_cast<std::size_t>(n));
  for (const auto& [u, v] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n) {
      continue;
    }
    adj[static_cast<std::size_t>(u)].push_back(v);
  }

  std::vector<char> seen(static_cast<std::size_t>(n), 0);
  std::stack<int> st;
  st.push(s);
  seen[static_cast<std::size_t>(s)] = 1;
  while (!st.empty()) {
    const int u = st.top();
    st.pop();
    if (u == t) {
      return 1;
    }
    for (int v : adj[static_cast<std::size_t>(u)]) {
      if (!seen[static_cast<std::size_t>(v)]) {
        seen[static_cast<std::size_t>(v)] = 1;
        st.push(v);
      }
    }
  }
  return 0;
}

std::string
test_dfs_reachable_cpp_output(int n,
                              const std::vector<std::pair<int, int>>& edges,
                              int s,
                              int t) {
  return std::to_string(test_dfs_reachable_cpp(n, edges, s, t)) + "\n";
}
