#pragma once

#include <string>
#include <vector>

// CLRS Ch.9 order statistic: return the i-th smallest element (0-based).
// Empty array or i out of range -> -1.
int
test_select_ith_cpp(std::vector<int> values, int i);

std::string
test_select_ith_cpp_output(const std::vector<int>& values, int i);
