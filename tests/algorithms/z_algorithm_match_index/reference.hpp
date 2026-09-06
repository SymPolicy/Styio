#pragma once

#include <string>
#include <vector>

// CLRS Ch.32 companion: Z-algorithm first match index of pattern in text.
// Empty pattern -> 0; no match -> -1.
int
test_z_algorithm_match_index_cpp(const std::vector<int>& text, const std::vector<int>& pattern);

std::string
test_z_algorithm_match_index_cpp_output(
  const std::vector<int>& text, const std::vector<int>& pattern);
