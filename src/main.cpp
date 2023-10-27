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
    std::string contents;

    std::string str;
    while (std::getline(file, str)) {
      contents += str;
      contents.push_back('\n'); }
    contents += EOF;

    // printf("%s", contents.c_str());
    return contents; }
  else {
    printf("Failed: Can't read file %s", filename);
  }

  return std::string("...");
}

int main(int argc, char* argv[]) {
  // std::copy(argv, argv + argc, std::ostream_iterator<char *>(std::cout, "\n"));

  // std::unique_ptr<llvm::LLVMContext> llvm_context = std::make_unique<llvm::LLVMContext>();
  // std::unique_ptr<llvm::Module> llvm_module = std::make_unique<llvm::Module>("styio", *llvm_context);
  // std::unique_ptr<llvm::IRBuilder<>> llvm_builder = std::make_unique<llvm::IRBuilder<>>(*llvm_context);

  std::ifstream file ( argv[1] );
  if ( !file.is_open() ) { printf("Failed: Can't open file %s.\n", argv[1]); }
  
  auto styio_program = parse_main_block(read_styio_file(argv[1]));

  auto generator = StyioToLLVM();
  generator.visit_main(&*styio_program);
  generator.show();
  
  return 0;
}