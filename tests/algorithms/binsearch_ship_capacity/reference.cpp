#include "reference.hpp"

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

namespace {

bool
feasible(int capacity, int days, const std::vector<int>& weights) {
  int used = 1;
  int load = 0;
  for (int w : weights) {
    if (w > capacity) {
      return false;
    }
    if (load + w > capacity) {
      ++used;
      load = 0;
    }
    load += w;
  }
  return used <= days;
}

} // namespace

int
test_binsearch_ship_capacity_cpp(int days, const std::vector<int>& weights) {
  if (days < 1 || weights.empty()) {
    return -1;
  }
  int lo = *std::max_element(weights.begin(), weights.end());
  int hi = std::accumulate(weights.begin(), weights.end(), 0);
  int ans = -1;
  while (lo <= hi) {
    const int mid = lo + (hi - lo) / 2;
    if (feasible(mid, days, weights)) {
      ans = mid;
      hi = mid - 1;
    } else {
      lo = mid + 1;
    }
  }
  return ans;
}

std::string
test_binsearch_ship_capacity_cpp_output(int days,
                                        const std::vector<int>& weights) {
  return std::to_string(test_binsearch_ship_capacity_cpp(days, weights)) + "\n";
}
