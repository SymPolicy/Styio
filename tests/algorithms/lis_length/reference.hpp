#pragma once

#include <string>
#include <vector>

// Longest increasing subsequence length (strict), classic O(n^2) DP.
int
test_lis_length_cpp(const std::vector<int>& values);

std::string
test_lis_length_cpp_output(const std::vector<int>& values);
