#pragma once

#include <string>
#include <tuple>

// CLRS 31.2 Extended-Euclid: returns (g, x, y) with a*x + b*y = g = gcd(|a|,|b|).
// Convention: work on absolute values of a,b; signs absorbed into coefficients.
// Empty/short input -> (0,0,0).
std::tuple<int, int, int>
test_extended_gcd_cpp(int a, int b);

std::string
test_extended_gcd_cpp_output(int a, int b);
