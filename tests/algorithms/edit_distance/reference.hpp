#pragma once

#include <string>
#include <vector>

// Classic Levenshtein edit distance (insert/delete/substitute cost 1).
// Companion to CLRS string/DP exercises; length of min edit script.
int
test_edit_distance_cpp(const std::vector<int>& a, const std::vector<int>& b);

std::string
test_edit_distance_cpp_output(const std::vector<int>& a, const std::vector<int>& b);
