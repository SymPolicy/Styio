#include "reference.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

int
test_closest_pair_dist_sq_cpp(const std::vector<std::pair<int, int>>& pts) {
  const int n = static_cast<int>(pts.size());
  if (n < 2) {
    return -1;
  }
  std::vector<std::pair<int, int>> px = pts;
  std::sort(px.begin(), px.end());
  auto dist2 = [](const std::pair<int, int>& a, const std::pair<int, int>& b) -> long long {
    const long long dx = 1LL * a.first - b.first;
    const long long dy = 1LL * a.second - b.second;
    return dx * dx + dy * dy;
  };
  // Divide-and-conquer classic (CLRS Ch.33)
  std::vector<std::pair<int, int>> scratch(static_cast<std::size_t>(n));
  auto solve = [&](auto&& self, int l, int r) -> long long {
    if (r - l <= 3) {
      long long best = LLONG_MAX;
      for (int i = l; i < r; ++i) {
        for (int j = i + 1; j < r; ++j) {
          best = std::min(best, dist2(px[static_cast<std::size_t>(i)],
                                      px[static_cast<std::size_t>(j)]));
        }
      }
      std::sort(px.begin() + l, px.begin() + r,
                [](const auto& a, const auto& b) { return a.second < b.second; });
      return best;
    }
    const int mid = (l + r) / 2;
    const int midx = px[static_cast<std::size_t>(mid)].first;
    long long d = std::min(self(self, l, mid), self(self, mid, r));
    std::merge(px.begin() + l, px.begin() + mid, px.begin() + mid, px.begin() + r,
               scratch.begin(),
               [](const auto& a, const auto& b) { return a.second < b.second; });
    std::copy(scratch.begin(), scratch.begin() + (r - l), px.begin() + l);
    std::vector<std::pair<int, int>> strip;
    for (int i = l; i < r; ++i) {
      const long long dx = 1LL * px[static_cast<std::size_t>(i)].first - midx;
      if (dx * dx < d) {
        strip.push_back(px[static_cast<std::size_t>(i)]);
      }
    }
    for (int i = 0; i < static_cast<int>(strip.size()); ++i) {
      for (int j = i + 1; j < static_cast<int>(strip.size()); ++j) {
        const long long dy =
          1LL * strip[static_cast<std::size_t>(j)].second -
          strip[static_cast<std::size_t>(i)].second;
        if (dy * dy >= d) {
          break;
        }
        d = std::min(d, dist2(strip[static_cast<std::size_t>(i)],
                              strip[static_cast<std::size_t>(j)]));
      }
    }
    return d;
  };
  const long long ans = solve(solve, 0, n);
  if (ans > 2000000000LL) {
    return 2000000000;
  }
  return static_cast<int>(ans);
}

std::string
test_closest_pair_dist_sq_cpp_output(const std::vector<std::pair<int, int>>& pts) {
  return std::to_string(test_closest_pair_dist_sq_cpp(pts)) + "\n";
}
