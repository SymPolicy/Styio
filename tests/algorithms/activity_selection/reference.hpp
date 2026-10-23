#pragma once

#include <string>
#include <utility>
#include <vector>

// CLRS 16.1 Activity selection: maximum number of compatible activities.
// Each activity is [start, finish) with start < finish.
int
test_activity_selection_cpp(std::vector<std::pair<int, int>> activities);

std::string
test_activity_selection_cpp_output(const std::vector<std::pair<int, int>>& activities);
