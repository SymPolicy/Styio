#pragma once

#include <string>
#include <utility>
#include <vector>

// Undirected articulation-point count (CLRS 22.5 / bridge companion).
// A vertex whose removal increases the number of connected components
// (among the remaining vertices). Isolated vertices are not articulation
// points. Malformed n<=0 -> 0.
int
test_articulation_points_count_cpp(
  int n, const std::vector<std::pair<int, int>>& edges);

std::string
test_articulation_points_count_cpp_output(
  int n, const std::vector<std::pair<int, int>>& edges);
