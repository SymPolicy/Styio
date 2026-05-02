#include "reference.hpp"

#include <algorithm>
#include <string>

std::pair<int, int>
test_minmax_pair_cpp(const std::vector<int>& values) {
  const auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
  if (min_it == values.end()) {
    return { -1, -1 };
  }
  return { *min_it, *max_it };
}

std::string
test_minmax_pair_cpp_output(const std::vector<int>& values) {
  const auto [min_value, max_value] = test_minmax_pair_cpp(values);
  return std::to_string(min_value) + "\n" + std::to_string(max_value) + "\n";
}
