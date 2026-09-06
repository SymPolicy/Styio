#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_matrix_determinant_cpp(const std::vector<std::vector<int>>& a) {
  const int n = static_cast<int>(a.size());
  if (n <= 0 || n > 6) {
    return 0;
  }
  std::vector<std::vector<long long>> m(static_cast<std::size_t>(n),
                                        std::vector<long long>(static_cast<std::size_t>(n)));
  for (int i = 0; i < n; ++i) {
    if (static_cast<int>(a[static_cast<std::size_t>(i)].size()) != n) {
      return 0;
    }
    for (int j = 0; j < n; ++j) {
      m[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
        a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
    }
  }
  long long sign = 1;
  long long prev = 1;
  for (int k = 0; k < n - 1; ++k) {
    int piv = k;
    while (piv < n && m[static_cast<std::size_t>(piv)][static_cast<std::size_t>(k)] == 0) {
      ++piv;
    }
    if (piv == n) {
      return 0;
    }
    if (piv != k) {
      std::swap(m[static_cast<std::size_t>(piv)], m[static_cast<std::size_t>(k)]);
      sign = -sign;
    }
    for (int i = k + 1; i < n; ++i) {
      for (int j = k + 1; j < n; ++j) {
        const long long val =
          m[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] *
            m[static_cast<std::size_t>(k)][static_cast<std::size_t>(k)] -
          m[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] *
            m[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)];
        m[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = val / prev;
      }
    }
    prev = m[static_cast<std::size_t>(k)][static_cast<std::size_t>(k)];
  }
  const long long det =
    sign * m[static_cast<std::size_t>(n - 1)][static_cast<std::size_t>(n - 1)];
  if (det > 2000000000LL) {
    return 2000000000;
  }
  if (det < -2000000000LL) {
    return -2000000000;
  }
  return static_cast<int>(det);
}

std::string
test_matrix_determinant_cpp_output(const std::vector<std::vector<int>>& a) {
  return std::to_string(test_matrix_determinant_cpp(a)) + "\n";
}
