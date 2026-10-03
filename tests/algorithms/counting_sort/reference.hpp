#pragma once

#include <string>
#include <vector>

// CLRS 8.2 Counting Sort over integers in 0..k with k = max(values) (or 0).
// Empty input -> empty output. Negative values are rejected -> empty output.
std::vector<int>
test_counting_sort_cpp(const std::vector<int>& values);

std::string
test_counting_sort_cpp_output(const std::vector<int>& values);
