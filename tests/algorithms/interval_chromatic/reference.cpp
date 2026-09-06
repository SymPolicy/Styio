#include "reference.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

int
test_interval_chromatic_cpp(const std::vector<std::pair<int, int>>& intervals) {
  std::vector<std::pair<int, int>> events;
  events.reserve(intervals.size() * 2);
  for (const auto& [s, f] : intervals) {
    if (s >= f) {
      continue;
    }
    events.push_back({s, +1});
    events.push_back({f, -1});
  }
  // Process ends before starts at the same coordinate (half-open).
  std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
    if (a.first != b.first) {
      return a.first < b.first;
    }
    return a.second < b.second;
  });
  int cur = 0;
  int best = 0;
  for (const auto& [x, d] : events) {
    (void)x;
    cur += d;
    if (cur > best) {
      best = cur;
    }
  }
  return best;
}

std::string
test_interval_chromatic_cpp_output(
  const std::vector<std::pair<int, int>>& intervals) {
  return std::to_string(test_interval_chromatic_cpp(intervals)) + "\n";
}
