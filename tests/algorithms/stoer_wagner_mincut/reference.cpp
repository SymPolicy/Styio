#include "reference.hpp"

#include <string>
#include <vector>

int
test_stoer_wagner_mincut_cpp(int n, const std::vector<int>& mat) {
  if (n <= 0) {
    return -1;
  }
  if (n == 1) {
    return 0;
  }
  if (static_cast<int>(mat.size()) < n * n) {
    return -1;
  }
  std::vector<std::vector<int>> g(static_cast<std::size_t>(n), std::vector<int>(static_cast<std::size_t>(n), 0));
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      int w = mat[static_cast<std::size_t>(i * n + j)];
      if (w < 0) {
        w = 0;
      }
      g[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = w;
    }
  }
  // symmetrize with max of both directions (undirected)
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      int w = g[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
      const int w2 = g[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)];
      if (w2 > w) {
        w = w2;
      }
      g[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = w;
      g[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] = w;
    }
    g[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] = 0;
  }
  const int INF = 2000000000;
  int best = INF;
  std::vector<int> v(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    v[static_cast<std::size_t>(i)] = i;
  }
  int nn = n;
  while (nn > 1) {
    std::vector<int> weight(static_cast<std::size_t>(nn), 0);
    std::vector<char> added(static_cast<std::size_t>(nn), 0);
    int prev = -1;
    for (int phase = 0; phase < nn; ++phase) {
      int sel = -1;
      for (int i = 0; i < nn; ++i) {
        if (!added[static_cast<std::size_t>(i)] &&
            (sel == -1 || weight[static_cast<std::size_t>(i)] > weight[static_cast<std::size_t>(sel)])) {
          sel = i;
        }
      }
      added[static_cast<std::size_t>(sel)] = 1;
      if (phase == nn - 1) {
        if (weight[static_cast<std::size_t>(sel)] < best) {
          best = weight[static_cast<std::size_t>(sel)];
        }
        // merge sel into prev
        const int a = v[static_cast<std::size_t>(prev)];
        const int b = v[static_cast<std::size_t>(sel)];
        for (int i = 0; i < nn; ++i) {
          const int vi = v[static_cast<std::size_t>(i)];
          g[static_cast<std::size_t>(a)][static_cast<std::size_t>(vi)] +=
            g[static_cast<std::size_t>(b)][static_cast<std::size_t>(vi)];
          g[static_cast<std::size_t>(vi)][static_cast<std::size_t>(a)] =
            g[static_cast<std::size_t>(a)][static_cast<std::size_t>(vi)];
        }
        g[static_cast<std::size_t>(a)][static_cast<std::size_t>(a)] = 0;
        v[static_cast<std::size_t>(sel)] = v[static_cast<std::size_t>(nn - 1)];
        --nn;
        break;
      }
      prev = sel;
      for (int i = 0; i < nn; ++i) {
        if (!added[static_cast<std::size_t>(i)]) {
          weight[static_cast<std::size_t>(i)] +=
            g[static_cast<std::size_t>(v[static_cast<std::size_t>(sel)])]
             [static_cast<std::size_t>(v[static_cast<std::size_t>(i)])];
        }
      }
    }
  }
  return best == INF ? 0 : best;
}

std::string
test_stoer_wagner_mincut_cpp_output(int n, const std::vector<int>& mat) {
  return std::to_string(test_stoer_wagner_mincut_cpp(n, mat)) + "\n";
}
