#pragma once

#include <string>

// CLRS 31.6 Modular-Exponentiation: (base^exp) mod m for m>0, exp>=0.
// Malformed (m<=0 or exp<0) -> -1. base may be negative; reduce into [0,m).
int
test_mod_pow_cpp(int base, int exp, int mod);

std::string
test_mod_pow_cpp_output(int base, int exp, int mod);
