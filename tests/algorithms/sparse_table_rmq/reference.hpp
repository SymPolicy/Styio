#pragma once

#include <string>
#include <vector>

// Sparse-table RMQ: minimum of a[L..R] (0-based inclusive).
// Malformed -> 0.
int
test_sparse_table_rmq_cpp(const std::vector<int>& a, int L, int R);

std::string
test_sparse_table_rmq_cpp_output(const std::vector<int>& a, int L, int R);
