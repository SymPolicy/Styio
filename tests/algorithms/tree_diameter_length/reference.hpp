#pragma once

#include <string>
#include <utility>
#include <vector>

// Undirected tree diameter in number of edges. Forest: max over components.
// Malformed / n<=0 -> -1.
int
test_tree_diameter_length_cpp(int n, const std::vector<std::pair<int, int>>& edges);

std::string
test_tree_diameter_length_cpp_output(int n, const std::vector<std::pair<int, int>>& edges);
