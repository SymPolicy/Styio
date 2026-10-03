#include "reference.hpp"

#include <algorithm>
#include <climits>
#include <string>
#include <vector>

int
test_hungarian_assignment_cost_cpp(const std::vector<std::vector<int>>& cost) {
  const int n = static_cast<int>(cost.size());
  if (n <= 0) {
    return 0;
  }
  // 1-indexed Hungarian for clarity
  const int INF = 1000000000;
  std::vector<std::vector<int>> a(static_cast<std::size_t>(n + 1),
                                  std::vector<int>(static_cast<std::size_t>(n + 1), 0));
  for (int i = 1; i <= n; ++i) {
    for (int j = 1; j <= n; ++j) {
      a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
        cost[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j - 1)];
    }
  }
  std::vector<int> u(static_cast<std::size_t>(n + 1), 0),
    v(static_cast<std::size_t>(n + 1), 0),
    p(static_cast<std::size_t>(n + 1), 0),
    way(static_cast<std::size_t>(n + 1), 0);
  for (int i = 1; i <= n; ++i) {
    p[0] = i;
    int j0 = 0;
    std::vector<int> minv(static_cast<std::size_t>(n + 1), INF);
    std::vector<char> used(static_cast<std::size_t>(n + 1), 0);
    do {
      used[static_cast<std::size_t>(j0)] = 1;
      const int i0 = p[static_cast<std::size_t>(j0)];
      int delta = INF;
      int j1 = 0;
      for (int j = 1; j <= n; ++j) {
        if (used[static_cast<std::size_t>(j)]) {
          continue;
        }
        const int cur = a[static_cast<std::size_t>(i0)][static_cast<std::size_t>(j)] -
                        u[static_cast<std::size_t>(i0)] - v[static_cast<std::size_t>(j)];
        if (cur < minv[static_cast<std::size_t>(j)]) {
          minv[static_cast<std::size_t>(j)] = cur;
          way[static_cast<std::size_t>(j)] = j0;
        }
        if (minv[static_cast<std::size_t>(j)] < delta) {
          delta = minv[static_cast<std::size_t>(j)];
          j1 = j;
        }
      }
      for (int j = 0; j <= n; ++j) {
        if (used[static_cast<std::size_t>(j)]) {
          u[static_cast<std::size_t>(p[static_cast<std::size_t>(j)])] += delta;
          v[static_cast<std::size_t>(j)] -= delta;
        } else {
          minv[static_cast<std::size_t>(j)] -= delta;
        }
      }
      j0 = j1;
    } while (p[static_cast<std::size_t>(j0)] != 0);
    do {
      const int j1 = way[static_cast<std::size_t>(j0)];
      p[static_cast<std::size_t>(j0)] = p[static_cast<std::size_t>(j1)];
      j0 = j1;
    } while (j0 != 0);
  }
  int ans = 0;
  for (int j = 1; j <= n; ++j) {
    ans += a[static_cast<std::size_t>(p[static_cast<std::size_t>(j)])]
             [static_cast<std::size_t>(j)];
  }
  return ans;
}

std::string
test_hungarian_assignment_cost_cpp_output(const std::vector<std::vector<int>>& cost) {
  return std::to_string(test_hungarian_assignment_cost_cpp(cost)) + "\n";
}
