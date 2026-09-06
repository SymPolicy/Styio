#include "reference.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

int
test_two_sat_flag_cpp(
  int nv, const std::vector<std::pair<int, int>>& clauses) {
  if (nv <= 0) {
    return 1;
  }
  const int N = 2 * nv;
  auto idx = [&](int lit) -> int {
    // lit > 0 => var lit-1 true at 2*(lit-1); lit < 0 => false at 2*(-lit-1)+1
    if (lit > 0) {
      return 2 * (lit - 1);
    }
    return 2 * ((-lit) - 1) + 1;
  };
  auto neg = [&](int node) -> int {
    return node ^ 1;
  };
  std::vector<std::vector<int>> g(static_cast<std::size_t>(N));
  std::vector<std::vector<int>> gr(static_cast<std::size_t>(N));
  auto add_imp = [&](int a, int b) {
    g[static_cast<std::size_t>(a)].push_back(b);
    gr[static_cast<std::size_t>(b)].push_back(a);
  };
  for (const auto& [la, lb] : clauses) {
    if (la == 0 || lb == 0 || la < -nv || la > nv || lb < -nv || lb > nv) {
      return 0;
    }
    const int a = idx(la);
    const int b = idx(lb);
    add_imp(neg(a), b);
    add_imp(neg(b), a);
  }
  std::vector<int> order;
  std::vector<int> used(static_cast<std::size_t>(N), 0);
  std::function<void(int)> dfs1 = [&](int u) {
    used[static_cast<std::size_t>(u)] = 1;
    for (int v : g[static_cast<std::size_t>(u)]) {
      if (!used[static_cast<std::size_t>(v)]) {
        dfs1(v);
      }
    }
    order.push_back(u);
  };
  for (int i = 0; i < N; ++i) {
    if (!used[static_cast<std::size_t>(i)]) {
      dfs1(i);
    }
  }
  std::vector<int> comp(static_cast<std::size_t>(N), -1);
  int cid = 0;
  std::function<void(int)> dfs2 = [&](int u) {
    comp[static_cast<std::size_t>(u)] = cid;
    for (int v : gr[static_cast<std::size_t>(u)]) {
      if (comp[static_cast<std::size_t>(v)] == -1) {
        dfs2(v);
      }
    }
  };
  for (int i = N - 1; i >= 0; --i) {
    const int u = order[static_cast<std::size_t>(i)];
    if (comp[static_cast<std::size_t>(u)] == -1) {
      dfs2(u);
      ++cid;
    }
  }
  for (int i = 0; i < nv; ++i) {
    if (comp[static_cast<std::size_t>(2 * i)] ==
        comp[static_cast<std::size_t>(2 * i + 1)]) {
      return 0;
    }
  }
  return 1;
}

std::string
test_two_sat_flag_cpp_output(
  int nv, const std::vector<std::pair<int, int>>& clauses) {
  return std::to_string(test_two_sat_flag_cpp(nv, clauses)) + "\n";
}
