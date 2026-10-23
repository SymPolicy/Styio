#pragma once

#include <string>
#include <vector>

// Polynomial multiply via FFT (complex), rounded to nearest i32.
// Inputs are small integer coefficient sequences.
// Product coeffs of a*b; tests compare multi-line coefficient stdout.
std::vector<int>
test_fft_poly_multiply_cpp(const std::vector<int>& a, const std::vector<int>& b);

std::string
test_fft_poly_multiply_cpp_output(const std::vector<int>& a, const std::vector<int>& b);
