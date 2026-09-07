#pragma once

#include <string>
#include <vector>

// Height of the Cartesian tree (min-heap, left-to-right inorder = array order).
// Empty -> 0; height of single node = 0 (edges on longest root-to-leaf).
int
test_cartesian_tree_height_cpp(const std::vector<int>& a);

std::string
test_cartesian_tree_height_cpp_output(const std::vector<int>& a);
