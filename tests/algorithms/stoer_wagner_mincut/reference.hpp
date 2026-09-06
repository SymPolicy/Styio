#pragma once

#include <string>
#include <vector>

// Global min-cut value (Stoer-Wagner). Undirected weighted graph as n x n matrix.
// n<=1 -> 0; n<=0 -> -1.
int
test_stoer_wagner_mincut_cpp(int n, const std::vector<int>& mat);

std::string
test_stoer_wagner_mincut_cpp_output(int n, const std::vector<int>& mat);
