#pragma once

#include <string>
#include <utility>
#include <vector>

// Closest pair of points: minimum squared Euclidean distance.
// n<2 -> -1.
int
test_closest_pair_dist_sq_cpp(const std::vector<std::pair<int, int>>& pts);

std::string
test_closest_pair_dist_sq_cpp_output(const std::vector<std::pair<int, int>>& pts);
