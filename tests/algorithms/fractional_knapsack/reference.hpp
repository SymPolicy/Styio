#pragma once

#include <string>
#include <utility>
#include <vector>

// CLRS 16.2 Fractional knapsack: maximum value with integer floor of the
// classic real-valued optimum when weights/values are integers
// (whole items + floor(v * remain / w) for the last fraction).
// Malformed W<0 -> 0; non-positive weight items skipped.
int
test_fractional_knapsack_cpp(int capacity, std::vector<std::pair<int, int>> items);

std::string
test_fractional_knapsack_cpp_output(int capacity,
                                   const std::vector<std::pair<int, int>>& items);
