#pragma once
#ifndef STYIO_UTILITY_H_
#define STYIO_UTILITY_H_

#include <string>
#include <iostream>

inline std::string make_padding(int indent, std::string endswith = "")
{
  return std::string("|") 
    + std::string(2 * indent, '-') 
    + std::string("|")  
    + endswith;
}

template <typename T>
void show_program (T& program) {
  std::cout << "\033[1;33m[>_<]\033[0m " << program -> toString() << "\n" << std::endl;
};

#endif