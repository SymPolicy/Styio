#pragma once

#include <string>
#include <vector>

// CLRS 15.1 rod-cutting: maximum revenue for a rod of length n.
// prices[i] is the price of a piece of length i+1 (i = 0 .. n-1).
int
test_rod_cutting_cpp(const std::vector<int>& prices);

std::string
test_rod_cutting_cpp_output(const std::vector<int>& prices);
