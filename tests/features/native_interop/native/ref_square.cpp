#include <vector>

extern "C" int ref_square(int x) {
  std::vector<int> values;
  values.push_back(x);
  return values[0] * values[0];
}
