// [C++ STL]
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

// [Styio]
#include "include/StyioAST/AST.hpp"
#include "include/StyioException/Exception.hpp"
#include "include/StyioParser/Parser.hpp"
#include "include/StyioToken/Token.hpp"
#include "include/StyioUtil/Util.hpp"

// [LLVM]
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

// [Others]
#include "include/Others/cxxopts.hpp" /* https://github.com/jarro2783/cxxopts */

void
show_cwd()
{
  std::filesystem::path cwd = std::filesystem::current_path();
  std::cout << cwd.string() << std::endl;
}

std::string
read_styio_file(const char* filename)
{
  if (std::filesystem::exists(filename)) {
    std::ifstream file(filename);
    if (!file.is_open()) {
      printf("Failed: Can't open file %s.\n", filename);
    }

    std::string code;
    std::string line;
    while (std::getline(file, line)) {
      code += line;
      code.push_back('\n');
    }
    code += EOF;
    return code;
  } else {
    printf("Failed: Can't read file %s.", filename);
  }

  return std::string("...");
}

int
main(int argc, char* argv[])
{
  cxxopts::Options options("styio", "Styio Compiler");

  options.add_options()(
    "f,file", "Source File Path", cxxopts::value<std::string>())(
    "a,ast", "Show Styio AST", cxxopts::value<bool>()->default_value("false"))(
    "i,ir", "Show LLVM IR", cxxopts::value<bool>()->default_value("false"))(
    "h,help", "Show All Command-Line Options");

  auto cmlopts = options.parse(argc, argv);

  if (cmlopts.count("help")) {
    std::cout << options.help() << std::endl;
    exit(0);
  }

  bool show_ast = cmlopts["ast"].as<bool>();
  bool show_ir = cmlopts["ir"].as<bool>();

  std::string fpath; /* File Path: fpath */
  if (cmlopts.count("file")) {
    fpath = cmlopts["file"].as<std::string>();
    // std::cout << fpath << std::endl;

    auto styio_code = read_styio_file(fpath.c_str());
    auto styio_context = std::make_shared<StyioContext>(styio_code);
    auto styio_program = parse_main_block(styio_context);

    if (show_ast) {
      show_program(styio_program);
    }

    if (show_ir) {
      auto generator = StyioToLLVM();
      generator.toLLVM(&*styio_program);
      generator.show();
    }
  }

  return 0;
}