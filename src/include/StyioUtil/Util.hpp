#pragma once
#ifndef STYIO_UTILITY_H_
#define STYIO_UTILITY_H_

#include <iostream>
#include <regex>
#include <string>
#include <vector>

inline std::string
make_padding(int indent, std::string endswith = "") {
  return std::string("|") + std::string(2 * indent, '-') + std::string("|") + endswith;
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
print_ast(T& program) {
  std::cout << "\033[1;32mAST\033[0m \033[31m--No-Type-Checking\033[0m" << "\n" << std::endl;
  std::cout << program->toString() << std::endl;
  std::cout << "\n" << std::endl;
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