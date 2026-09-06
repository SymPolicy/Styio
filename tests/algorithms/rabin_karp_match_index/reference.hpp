#pragma once

#include <string>
#include <vector>

// CLRS 32.2 Rabin-Karp: first index of pattern in text, or -1.
// Rolling hash with base 256 and prime 101; verify on hash hit.
// Empty pattern matches at 0.
int
test_rabin_karp_match_index_cpp(const std::vector<int>& text,
                                const std::vector<int>& pattern);

std::string
test_rabin_karp_match_index_cpp_output(const std::vector<int>& text,
                                       const std::vector<int>& pattern);
