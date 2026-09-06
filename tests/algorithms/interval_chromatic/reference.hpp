#pragma once

#include <string>
#include <utility>
#include <vector>

// Interval-graph chromatic number = maximum depth of open intervals.
// Intervals are half-open [s, f) with s < f. Empty -> 0. Malformed -> 0.
int
test_interval_chromatic_cpp(const std::vector<std::pair<int, int>>& intervals);

std::string
test_interval_chromatic_cpp_output(
  const std::vector<std::pair<int, int>>& intervals);
