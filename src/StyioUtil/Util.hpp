#pragma once
#ifndef STYIO_UTILITY_H_
#define STYIO_UTILITY_H_

#include <iostream>
#include <regex>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

// inline std::string
// make_padding(int indent, std::string endswith = "") {
//   return std::string("|") + std::string(2 * indent, '-') + std::string("|") + endswith;
// }

/** When stdout is not a tty (e.g. piped to FileCheck), skip ANSI escapes for stable tests. */
inline bool
styio_stdout_plain() {
#if defined(_WIN32)
  return _isatty(_fileno(stdout)) == 0;
#else
  return !isatty(STDOUT_FILENO);
#endif
}

inline std::string
make_padding(int indent, std::string endswith = "") {
  return std::string(2 * indent, ' ') + std::string("|- ") + endswith;
}

template <typename T>
void
show_vector(std::vector<T> v) {
  std::cout << "[ ";
  for (auto a : v) {
    std::cout << a << " ";
  }
  std::cout << "]" << std::endl;
};

template <typename T>
void
print_ast(T& program, bool after_check) {
  if (after_check) {
    std::cout 
      << "\033[1;32mAST\033[0m \033[1;33m-After-Type-Checking\033[0m"
      << "\n"
      << program->toString() 
      << "\n"
      << std::endl;
  }
  else {
    std::cout 
      << "\033[1;32mAST\033[0m \033[31m-No-Type-Checking\033[0m" 
      << "\n" 
      << program->toString() 
      << "\n" << std::endl;
  }
};

inline bool
is_ipv4_at_start(const std::string& str) {
  std::regex ipv4Regex(
    "^(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
    "(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
    "(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
    "(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"
  );
  return std::regex_search(str, ipv4Regex, std::regex_constants::match_continuous);
}

inline bool
is_ipv6_at_start(const std::string& str) {
  std::regex ipv6Regex("^(?:[A-Fa-f0-9]{1,4}:){7}[A-Fa-f0-9]{1,4}");
  return std::regex_search(str, ipv6Regex, std::regex_constants::match_continuous);
}

#endif