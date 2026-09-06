#pragma once

#include <string>

// Integer partition function p(n): number of unrestricted partitions of n.
// Classic DP companion to CLRS Ch.15. p(0)=1. Malformed n<0 -> -1.
int
test_integer_partition_count_cpp(int n);

std::string
test_integer_partition_count_cpp_output(int n);
