#include "reference.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

int
test_fractional_knapsack_cpp(int capacity, std::vector<std::pair<int, int>> items) {
  if (capacity <= 0) {
    return 0;
  }
  items.erase(std::remove_if(items.begin(), items.end(),
                             [](const std::pair<int, int>& it) {
                               return it.first <= 0 || it.second < 0;
                             }),
              items.end());
  std::sort(items.begin(), items.end(),
            [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
              // density a.v/a.w >= b.v/b.w  <=>  a.v*b.w >= b.v*a.w
              return static_cast<long long>(a.second) * b.first >
                     static_cast<long long>(b.second) * a.first;
            });
  int remain = capacity;
  int total = 0;
  for (const auto& [w, v] : items) {
    if (remain <= 0) {
      break;
    }
    if (w <= remain) {
      total += v;
      remain -= w;
    } else {
      total += static_cast<int>((static_cast<long long>(v) * remain) / w);
      remain = 0;
    }
  }
  return total;
}

std::string
test_fractional_knapsack_cpp_output(int capacity,
                                   const std::vector<std::pair<int, int>>& items) {
  return std::to_string(test_fractional_knapsack_cpp(capacity, items)) + "\n";
}
