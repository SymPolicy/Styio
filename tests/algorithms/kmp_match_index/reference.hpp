#pragma once

#include <string>
#include <vector>

// CLRS 32.4 Knuth-Morris-Pratt: first index of pattern in text, or -1.
// Empty pattern matches at 0 (like std::search). Short/malformed handled
// by the caller encoding; empty text with non-empty pattern -> -1.
int
test_kmp_match_index_cpp(const std::vector<int>& text,
                         const std::vector<int>& pattern);

std::string
test_kmp_match_index_cpp_output(const std::vector<int>& text,
                                const std::vector<int>& pattern);
