#pragma once

#include <string>
#include <vector>

// Unbounded coin-change: minimum number of coins to make `amount`
// (CLRS DP chapter companion / classic exercise). Coins are positive.
// Impossible -> -1; malformed amount<0 or empty usable coins with amount>0 -> -1;
// amount==0 -> 0.
int
test_coin_change_min_cpp(int amount, const std::vector<int>& coins);

std::string
test_coin_change_min_cpp_output(int amount, const std::vector<int>& coins);
