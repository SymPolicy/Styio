#pragma once

#include <string>
#include <vector>

// Fenwick / Binary Indexed Tree inclusive range sum a[L..R] (0-based).
// Malformed bounds / n<=0 -> 0.
int
test_fenwick_range_sum_cpp(const std::vector<int>& a, int L, int R);

std::string
test_fenwick_range_sum_cpp_output(const std::vector<int>& a, int L, int R);
