#include "reference.hpp"

#include <queue>
#include <string>
#include <utility>
#include <vector>

std::vector<int>
test_topo_order_cpp(int n, const std::vector<std::pair<int, int>>& edges) {
  if (n <= 0) {
    return {};
  }
  std::vector<std::vector<int>> adj(static_cast<std::size_t>(n));
  std::vector<int> indeg(static_cast<std::size_t>(n), 0);
  for (const auto& [u, v] : edges) {
    if (u < 0 || u >= n || v < 0 || v >= n) {
      continue;
    }
    adj[static_cast<std::size_t>(u)].push_back(v);
    ++indeg[static_cast<std::size_t>(v)];
  }
  std::priority_queue<int, std::vector<int>, std::greater<int>> ready;
  for (int i = 0; i < n; ++i) {
    if (indeg[static_cast<std::size_t>(i)] == 0) {
      ready.push(i);
    }
  }
  std::vector<int> order;
  order.reserve(static_cast<std::size_t>(n));
  while (!ready.empty()) {
    const int u = ready.top();
    ready.pop();
    order.push_back(u);
    for (int v : adj[static_cast<std::size_t>(u)]) {
      if (--indeg[static_cast<std::size_t>(v)] == 0) {
        ready.push(v);
      }
    }
  }
  if (static_cast<int>(order.size()) != n) {
    return {};
  }
  return order;
}

std::string
test_topo_order_cpp_output(int n, const std::vector<std::pair<int, int>>& edges) {
  const auto order = test_topo_order_cpp(n, edges);
  std::string out;
  for (int v : order) {
    out += std::to_string(v);
    out += '\n';
  }
  return out;
}
