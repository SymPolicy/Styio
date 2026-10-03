#pragma once

#include <string>

// Count of primes in [1..n] via Eratosthenes. n<=0 -> 0; n> workspace limit handled by caller.
int
test_sieve_prefix_prime_count_cpp(int n);

std::string
test_sieve_prefix_prime_count_cpp_output(int n);
