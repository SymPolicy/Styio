#pragma once

#include <string>
#include <vector>

// Manacher: length of longest palindromic contiguous subarray (i32 alphabet).
// Empty -> 0.
int
test_manacher_palindrome_length_cpp(const std::vector<int>& a);

std::string
test_manacher_palindrome_length_cpp_output(const std::vector<int>& a);
