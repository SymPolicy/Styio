// [C++ STL]
#include <type_traits>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <string>
#include <tuple>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>

// [Styio]
#include "include/StyioException/Exception.hpp"
#include "include/StyioToken/Token.hpp"
#include "include/StyioUtil/Util.hpp"
#include "include/StyioAST/AST.hpp"
#include "include/StyioParser/Parser.hpp"

void show_cwd() 
{
  std::filesystem::path cwd = std::filesystem::current_path();
  std::cout << cwd.string() << std::endl;
}

int main() {
  parse_program(read_styio_file("_.styio"));
  
  return 0;
}