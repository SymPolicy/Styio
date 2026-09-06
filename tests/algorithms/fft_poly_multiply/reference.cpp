#include "reference.hpp"

#include <cmath>
#include <complex>
#include <string>
#include <utility>
#include <vector>

namespace {

using Cd = std::complex<double>;

void
fft(std::vector<Cd>& a, bool invert) {
  const int n = static_cast<int>(a.size());
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      std::swap(a[static_cast<std::size_t>(i)], a[static_cast<std::size_t>(j)]);
    }
  }
  for (int len = 2; len <= n; len <<= 1) {
    const double ang = 2 * std::acos(-1.0) / len * (invert ? -1.0 : 1.0);
    const Cd wlen(std::cos(ang), std::sin(ang));
    for (int i = 0; i < n; i += len) {
      Cd w(1.0, 0.0);
      for (int j = 0; j < len / 2; ++j) {
        Cd u = a[static_cast<std::size_t>(i + j)];
        Cd v = a[static_cast<std::size_t>(i + j + len / 2)] * w;
        a[static_cast<std::size_t>(i + j)] = u + v;
        a[static_cast<std::size_t>(i + j + len / 2)] = u - v;
        w *= wlen;
      }
    }
  }
  if (invert) {
    for (Cd& x : a) {
      x /= n;
    }
  }
}

std::string
format_list(const std::vector<int>& values) {
  std::string out = "[";
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out += ",";
    }
    out += std::to_string(values[i]);
  }
  out += "]\n";
  return out;
}

} // namespace

std::vector<int>
test_fft_poly_multiply_cpp(const std::vector<int>& a, const std::vector<int>& b) {
  if (a.empty() || b.empty()) {
    return {};
  }
  int n = 1;
  while (n < static_cast<int>(a.size() + b.size())) {
    n <<= 1;
  }
  std::vector<Cd> fa(a.begin(), a.end());
  std::vector<Cd> fb(b.begin(), b.end());
  fa.resize(static_cast<std::size_t>(n));
  fb.resize(static_cast<std::size_t>(n));
  fft(fa, false);
  fft(fb, false);
  for (int i = 0; i < n; ++i) {
    fa[static_cast<std::size_t>(i)] *= fb[static_cast<std::size_t>(i)];
  }
  fft(fa, true);
  const int out_n = static_cast<int>(a.size() + b.size() - 1);
  std::vector<int> res(static_cast<std::size_t>(out_n));
  for (int i = 0; i < out_n; ++i) {
    res[static_cast<std::size_t>(i)] =
      static_cast<int>(std::llround(fa[static_cast<std::size_t>(i)].real()));
  }
  return res;
}

std::string
test_fft_poly_multiply_cpp_output(const std::vector<int>& a, const std::vector<int>& b) {
  return format_list(test_fft_poly_multiply_cpp(a, b));
}
