// [C++ STL]
#include <type_traits>
#include <algorithm>
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

// [LLVM]
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

void show_cwd() 
{
  std::filesystem::path cwd = std::filesystem::current_path();
  std::cout << cwd.string() << std::endl;
}

std::string read_styio_file(const char* filename) {
  if (std::filesystem::exists(filename)) {
    std::ifstream file(filename);
    if ( !file.is_open() ) { printf("Failed: Can't open file %s.\n", filename); }

    std::string code;
    std::string line;
    while (std::getline(file, line)) {
      code += line;
      code.push_back('\n'); }
    code += EOF;
    return code; }
  else {
    printf("Failed: Can't read file %s.", filename);
  }

  return std::string("...");
}

int main(int argc, char* argv[]) {
  // std::copy(argv, argv + argc, std::ostream_iterator<char *>(std::cout, "\n"));

  auto styio_code = read_styio_file(argv[1]);
  auto styio_context = std::make_shared<StyioContext>(styio_code);
  auto styio_program = parse_main_block(styio_context);

  auto checker = StyioChecker();
  checker.visit_main(&*styio_program);

  auto generator = StyioToLLVM();
  generator.visit_main(&*styio_program);
  generator.show();
  
  return 0;
}