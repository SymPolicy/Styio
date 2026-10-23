#pragma once

#include <string>
#include <vector>

// LCP of suffixes starting at i and j via SA + Kasai (+ naive RMQ on height).
// Malformed indices / empty -> -1; i==j -> n-i.
int
test_suffix_array_lcp_cpp(const std::vector<int>& s, int i, int j);

std::string
test_suffix_array_lcp_cpp_output(const std::vector<int>& s, int i, int j);
