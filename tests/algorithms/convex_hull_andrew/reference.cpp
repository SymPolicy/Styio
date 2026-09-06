#include "reference.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

std::vector<std::pair<int, int>>
test_convex_hull_andrew_cpp(const std::vector<std::pair<int, int>>& pts) {
  std::vector<std::pair<int, int>> p = pts;
  if (p.empty()) {
    return {};
  }
  std::sort(p.begin(), p.end());
  p.erase(std::unique(p.begin(), p.end()), p.end());
  if (p.size() == 1) {
    return p;
  }
  auto cross = [](const std::pair<int, int>& o,
                  const std::pair<int, int>& a,
                  const std::pair<int, int>& b) -> long long {
    return 1LL * (a.first - o.first) * (b.second - o.second) -
           1LL * (a.second - o.second) * (b.first - o.first);
  };
  std::vector<std::pair<int, int>> lower;
  for (const auto& pt : p) {
    while (lower.size() >= 2 &&
           cross(lower[lower.size() - 2], lower[lower.size() - 1], pt) <= 0) {
      lower.pop_back();
    }
    lower.push_back(pt);
  }
  std::vector<std::pair<int, int>> upper;
  for (auto it = p.rbegin(); it != p.rend(); ++it) {
    while (upper.size() >= 2 &&
           cross(upper[upper.size() - 2], upper[upper.size() - 1], *it) <= 0) {
      upper.pop_back();
    }
    upper.push_back(*it);
  }
  lower.pop_back();
  upper.pop_back();
  lower.insert(lower.end(), upper.begin(), upper.end());
  return lower;
}

std::string
test_convex_hull_andrew_cpp_output(const std::vector<std::pair<int, int>>& pts) {
  const auto hull = test_convex_hull_andrew_cpp(pts);
  std::string out;
  for (const auto& [x, y] : hull) {
    out += std::to_string(x);
    out += "\n";
    out += std::to_string(y);
    out += "\n";
  }
  return out;
}
