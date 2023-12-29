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
#include <regex>
#include <utility>

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

struct code
{
  size_t length;
  std::string text;
  std::vector<std::pair<size_t, size_t>> linesep;
};


void
show_cwd() {
  std::filesystem::path cwd = std::filesystem::current_path();
  std::cout << cwd.string() << std::endl;
}

/*
  linenum_map:

  O(n)
  foreach {
    (0, a1): 1,
    (a1 + 1, a2): 2,
    (a2 + 1, a3): 3,
    ...
  }

  O(logN)
  vector<pair<size_t, size_t>>
  [0, a1, a2, ..., an]

  l: total length of the code
  n: last line number
  p: current position
  
  p < (l / 2) 
  then [0 ~ l/2]
  else [l/2 ~ l]

  what if two consecutive "\n"?
  that is: "\n\n"
*/

code
read_styio_file(
  const char* filename
) {
  std::string text = "";
  std::vector<std::pair<size_t, size_t>> linesep;

  if (std::filesystem::exists(filename)) {
    std::ifstream file(filename);
    if (!file.is_open()) {
      printf("Failed: Can't open file %s.\n", filename);
    }
    
    size_t p = 0;
    std::string line;
    while (std::getline(file, line)) {
      text += line;
      linesep.push_back(std::make_pair(p, line.size()));
      p += line.size();

      text.push_back('\n');
      p += 1;
    }
    text += EOF;
  }
  else {
    text = std::string("...");
    printf("Failed: Can't read file %s.", filename);
  }

  size_t total_length = text.size();

  struct code result = {total_length, text, linesep};
  return result;
}

void
show_code_with_linenum(code c) {
  auto& text = c.text;
  auto& linesep = c.linesep;

  for (size_t i = 0; i < linesep.size(); i++)
  {
    std::string line = text.substr(linesep.at(i).first, linesep.at(i).second);

    std::regex newline_regex ("\n");
    std::string replaced_text = std::regex_replace(line, newline_regex, "[NEWLINE]");

    std::cout 
      << "|" << i + 1 << "|-[" << linesep.at(i).first << ":" << (linesep.at(i).first + linesep.at(i).second - 1) << "] "
      << line << std::endl;
  }
};

int
main(
  int argc,
  char* argv[]
) {
  cxxopts::Options options("styio", "Styio Compiler");

  options.add_options()(
    "f,file", "Source File Path", cxxopts::value<std::string>()
  )(
    "a,ast", "Show Styio AST", cxxopts::value<bool>()->default_value("false")
  )(
    "i,ir", "Show LLVM IR", cxxopts::value<bool>()->default_value("false")
  )(
    "h,help", "Show All Command-Line Options"
  );

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
    show_code_with_linenum(styio_code);
    auto styio_context = std::make_shared<StyioContext>(styio_code.text);
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