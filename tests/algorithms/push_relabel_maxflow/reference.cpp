#include "reference.hpp"

#include <algorithm>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

int
test_push_relabel_maxflow_cpp(
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
  std::vector<std::vector<int>> flow(static_cast<std::size_t>(n),
                                     std::vector<int>(static_cast<std::size_t>(n), 0));
  std::vector<int> height(static_cast<std::size_t>(n), 0);
  std::vector<int> excess(static_cast<std::size_t>(n), 0);
  height[static_cast<std::size_t>(s)] = n;
  for (int v = 0; v < n; ++v) {
    if (cap[static_cast<std::size_t>(s)][static_cast<std::size_t>(v)] > 0) {
      flow[static_cast<std::size_t>(s)][static_cast<std::size_t>(v)] =
        cap[static_cast<std::size_t>(s)][static_cast<std::size_t>(v)];
      flow[static_cast<std::size_t>(v)][static_cast<std::size_t>(s)] =
        -cap[static_cast<std::size_t>(s)][static_cast<std::size_t>(v)];
      excess[static_cast<std::size_t>(v)] +=
        cap[static_cast<std::size_t>(s)][static_cast<std::size_t>(v)];
      excess[static_cast<std::size_t>(s)] -=
        cap[static_cast<std::size_t>(s)][static_cast<std::size_t>(v)];
    }
  }
  auto residual = [&](int u, int v) {
    return cap[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] -
           flow[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)];
  };
  auto push = [&](int u, int v) {
    const int delta = std::min(excess[static_cast<std::size_t>(u)], residual(u, v));
    flow[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] += delta;
    flow[static_cast<std::size_t>(v)][static_cast<std::size_t>(u)] -= delta;
    excess[static_cast<std::size_t>(u)] -= delta;
    excess[static_cast<std::size_t>(v)] += delta;
  };
  auto relabel = [&](int u) {
    int mh = 2000000000;
    for (int v = 0; v < n; ++v) {
      if (residual(u, v) > 0) {
        mh = std::min(mh, height[static_cast<std::size_t>(v)]);
      }
    }
    if (mh < 2000000000) {
      height[static_cast<std::size_t>(u)] = mh + 1;
    }
  };
  std::vector<int> list;
  for (int i = 0; i < n; ++i) {
    if (i != s && i != t) {
      list.push_back(i);
    }
  }
  std::size_t idx = 0;
  while (idx < list.size()) {
    const int u = list[idx];
    const int old_h = height[static_cast<std::size_t>(u)];
    while (excess[static_cast<std::size_t>(u)] > 0) {
      bool pushed = false;
      for (int v = 0; v < n; ++v) {
        if (excess[static_cast<std::size_t>(u)] == 0) {
          break;
        }
        if (residual(u, v) > 0 &&
            height[static_cast<std::size_t>(u)] ==
              height[static_cast<std::size_t>(v)] + 1) {
          push(u, v);
          pushed = true;
        }
      }
      if (!pushed) {
        relabel(u);
        if (height[static_cast<std::size_t>(u)] == old_h &&
            excess[static_cast<std::size_t>(u)] > 0) {
          // no residual downhill; break to avoid infinite loop on disconnected
          break;
        }
        if (height[static_cast<std::size_t>(u)] > old_h) {
          break;
        }
      }
    }
    if (height[static_cast<std::size_t>(u)] > old_h) {
      list.erase(list.begin() + static_cast<std::ptrdiff_t>(idx));
      list.insert(list.begin(), u);
      idx = 0;
    } else {
      ++idx;
    }
  }
  int maxflow = 0;
  for (int v = 0; v < n; ++v) {
    maxflow += flow[static_cast<std::size_t>(s)][static_cast<std::size_t>(v)];
  }
  return maxflow;
}

std::string
test_push_relabel_maxflow_cpp_output(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges) {
  return std::to_string(test_push_relabel_maxflow_cpp(n, s, t, edges)) + "\n";
}
