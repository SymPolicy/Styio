#pragma once

#include <string>
#include <utility>
#include <vector>

// Maximum independent set size on a forest rooted implicitly.
// n<=0 -> -1.
int
test_tree_mis_size_cpp(int n, const std::vector<std::pair<int, int>>& edges);

std::string
test_tree_mis_size_cpp_output(int n, const std::vector<std::pair<int, int>>& edges);
