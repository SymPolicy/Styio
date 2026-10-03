#pragma once

#include <string>
#include <vector>

// Boyer-Moore-Horspool first match index (bad-character shift).
// Empty pattern -> 0; no match -> -1. Alphabet values in tests are 0..15.
int
test_boyer_moore_match_index_cpp(
  const std::vector<int>& text, const std::vector<int>& pattern);

std::string
test_boyer_moore_match_index_cpp_output(
  const std::vector<int>& text, const std::vector<int>& pattern);
