#include "reference.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

int
test_activity_selection_cpp(std::vector<std::pair<int, int>> activities) {
  activities.erase(
    std::remove_if(activities.begin(), activities.end(),
                   [](const std::pair<int, int>& a) { return a.first >= a.second; }),
    activities.end());
  std::sort(activities.begin(), activities.end(),
            [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
              return a.second < b.second;
            });
  int count = 0;
  int last_finish = -1000000000;
  for (const auto& [s, f] : activities) {
    if (s >= last_finish) {
      ++count;
      last_finish = f;
    }
  }
  return count;
}

std::string
test_activity_selection_cpp_output(const std::vector<std::pair<int, int>>& activities) {
  return std::to_string(test_activity_selection_cpp(activities)) + "\n";
}
