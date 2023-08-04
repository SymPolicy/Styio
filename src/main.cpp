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

std::string read_styio_file(const char* filename)
{
  if (std::filesystem::exists(filename))
  {
    std::ifstream file(filename);
    std::string contents;

    std::string str;
    while (std::getline(file, str))
    {
      contents += str;
      contents.push_back('\n');
    }

    contents += EOF;

    // printf("%s", contents.c_str());

    return contents;
  }

  printf("Failed...");

  return std::string("...");
}

int main(int argc, char* argv[]) {
  std::ifstream file ( argv[1] );
  // Always check to see if file opening succeeded
  if ( !file.is_open() )
    printf("Failed: Can't open file %s.\n", argv[1]);
  else {
    parse_program(read_styio_file(argv[1]));
  }
  
  return 0;
}