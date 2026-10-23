#pragma once

#include <string>
#include <utility>
#include <vector>

// Sum of distinct counts over queries (C++ Mo's algorithm).
int
test_mos_range_distinct_cpp(
  const std::vector<int>& a, const std::vector<std::pair<int, int>>& queries);

std::string
test_mos_range_distinct_cpp_output(
  const std::vector<int>& a, const std::vector<std::pair<int, int>>& queries);
