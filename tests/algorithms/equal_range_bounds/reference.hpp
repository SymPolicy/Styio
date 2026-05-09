#pragma once

#include <string>
#include <utility>
#include <vector>

std::pair<int, int> test_equal_range_bounds_cpp(
  const std::vector<int>& values,
  int target);
std::string test_equal_range_bounds_cpp_output(
  const std::vector<int>& values,
  int target);
