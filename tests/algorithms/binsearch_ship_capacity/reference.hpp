#pragma once

#include <string>
#include <vector>

// Binary search on answer: minimum capacity to ship packages in order
// within `days` days (classic CLRS-style parametric search exercise).
// packages weights > 0. days < 1 or empty -> -1. Impossible -> -1.
int
test_binsearch_ship_capacity_cpp(int days, const std::vector<int>& weights);

std::string
test_binsearch_ship_capacity_cpp_output(int days,
                                        const std::vector<int>& weights);
