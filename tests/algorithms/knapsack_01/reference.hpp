#pragma once

#include <string>
#include <vector>

// Classic 0-1 knapsack (CLRS exercises / DP chapter companion):
// maximum value with capacity W; weights[i], values[i] for item i.
int
test_knapsack_01_cpp(int capacity,
                     const std::vector<int>& weights,
                     const std::vector<int>& values);

std::string
test_knapsack_01_cpp_output(int capacity,
                            const std::vector<int>& weights,
                            const std::vector<int>& values);
