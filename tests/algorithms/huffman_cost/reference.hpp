#pragma once

#include <string>
#include <vector>

// CLRS 16.3 Huffman: minimum weighted external path length for positive
// frequencies (sum over merges of combined weight). n<=1 -> 0.
int
test_huffman_cost_cpp(std::vector<int> freqs);

std::string
test_huffman_cost_cpp_output(const std::vector<int>& freqs);
