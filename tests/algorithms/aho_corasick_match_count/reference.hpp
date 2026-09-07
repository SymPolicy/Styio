#pragma once

#include <string>
#include <vector>

// Total occurrences of k patterns in text (overlaps counted; C++ Aho-Corasick).
int
test_aho_corasick_match_count_cpp(
  const std::vector<int>& text, const std::vector<std::vector<int>>& patterns);

std::string
test_aho_corasick_match_count_cpp_output(
  const std::vector<int>& text, const std::vector<std::vector<int>>& patterns);
