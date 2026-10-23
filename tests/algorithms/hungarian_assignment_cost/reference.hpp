#pragma once

#include <string>
#include <vector>

// Hungarian (Kuhn-Munkres) minimum assignment cost for dense n x n matrix.
// n<=0 -> 0.
int
test_hungarian_assignment_cost_cpp(const std::vector<std::vector<int>>& cost);

std::string
test_hungarian_assignment_cost_cpp_output(const std::vector<std::vector<int>>& cost);
