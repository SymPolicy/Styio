/*                                   */
/*    __| __ __| \ \  / _ _|   _ \   */
/*  \__ \    |    \  /    |   (   |  */
/*  ____/   _|     _|   ___| \___/   */
/*                                   */

// [C++ STL]
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// [Styio]
#include "StyioAST/AST.hpp"
#include "StyioAnalyzer/ASTAnalyzer.hpp"   /* StyioASTAnalyzer */
#include "StyioCodeGen/CodeGenVisitor.hpp" /* StyioToLLVMIR Code Generator */
#include "StyioException/Exception.hpp"
#include "StyioIR/StyioIR.hpp" /* StyioIR */
#include "StyioParser/Parser.hpp"
#include "StyioToString/ToStringVisitor.hpp" /* StyioRepr */
#include "StyioToken/Token.hpp"
#include "StyioUtil/Util.hpp"

// [LLVM]
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Error.h" /* ExitOnErr */
#include "llvm/Support/FileSystem.h"

// [Styio LLVM ORC JIT]
#include "StyioJIT/StyioJIT_ORC.hpp"

// [Others]
#include "include/cxxopts.hpp" /* https://github.com/jarro2783/cxxopts */

extern "C" void
hello_world() {
  std::cout << "hello, world!" << std::endl;
}

struct tmp_code_wrap
{
  std::string code_text;
  std::vector<std::pair<size_t, size_t>> line_seps;
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

tmp_code_wrap
read_styio_file(
  std::string file_path
) {
  std::string text = "";
  std::vector<std::pair<size_t, size_t>> lineseps;

  const char* fpath_cstr = file_path.c_str();
  if (std::filesystem::exists(fpath_cstr)) {
    std::ifstream file(fpath_cstr);
    if (!file.is_open()) {
      printf("Error: Can't open file %s.\n", fpath_cstr);
    }

    size_t p = 0;
    std::string line;
    while (std::getline(file, line)) {
      text += line;
      lineseps.push_back(std::make_pair(p, line.size()));
      p += line.size();

      text.push_back('\n');
      p += 1;
    }
    text += EOF;
  }
  else {
    text = std::string("...");
    printf("Error: File %s not found.", fpath_cstr);
  }

  struct tmp_code_wrap result = {text, lineseps};
  return result;
}

void
show_code_with_linenum(tmp_code_wrap c) {
  auto& code = c.code_text;
  auto& lineseps = c.line_seps;

  for (size_t i = 0; i < lineseps.size(); i++) {
    std::string line = code.substr(lineseps.at(i).first, lineseps.at(i).second);

    std::regex newline_regex("\n");
    std::string replaced_text = std::regex_replace(line, newline_regex, "[NEWLINE]");

    std::cout
      << "|" << i << "|-[" << lineseps.at(i).first << ":" << (lineseps.at(i).first + lineseps.at(i).second) << "] "
      << line << std::endl;
  }
};

int
main(
  int argc,
  char* argv[]
) {
  cxxopts::Options options("styio", "Styio Compiler");

  // styio example.styio
  options.allow_unrecognised_options();

  options.add_options()(
    "f,file", "Take the given source file.", cxxopts::value<std::string>()
  )(
    "h,help", "Show All Command-Line Options"
  );

  options.add_options()(
    "a,all", "Show All: Styio AST, Styio IR, LLVM IR", cxxopts::value<bool>()->default_value("false")
  )(
    "styio-ast", "Show Styio AST", cxxopts::value<bool>()->default_value("false")
  )(
    "styio-ir", "Show Styio IR", cxxopts::value<bool>()->default_value("false")
  )(
    "llvm-ir", "Show LLVM IR", cxxopts::value<bool>()->default_value("false")
  );

  options.add_options()(
    "debug", "Debug Mode", cxxopts::value<bool>()->default_value("false")
  );

  options.parse_positional({"file"});

  auto cmlopts = options.parse(argc, argv);

  if (cmlopts.count("help")) {
    std::cout << options.help() << std::endl;
    exit(0);
  }

  bool show_all = cmlopts["all"].as<bool>();

  bool show_styio_ast = cmlopts["styio-ast"].as<bool>();
  bool show_styio_ir = cmlopts["styio-ir"].as<bool>();
  bool show_llvm_ir = cmlopts["llvm-ir"].as<bool>();

  bool is_debug_mode = cmlopts["debug"].as<bool>();

  std::string fpath; /* File Path */
  if (cmlopts.count("file")) {
    fpath = cmlopts["file"].as<std::string>();
    // std::cout << fpath << std::endl;

    auto styio_code = read_styio_file(fpath);
    if (is_debug_mode) {
      show_code_with_linenum(styio_code);
    }

    auto styio_context = StyioContext::Create(
      fpath, 
      styio_code.code_text, 
      styio_code.line_seps,
      is_debug_mode /* is debug mode */);

    StyioRepr styio_repr = StyioRepr();

    /* Parser */
    auto styio_ast = parse_main_block(*styio_context);

    if (show_all or show_styio_ast) {
      std::cout
        << "\033[1;32mAST\033[0m \033[31m-No-Type-Checking\033[0m"
        << "\n"
        << styio_ast->toString(&styio_repr) << "\n"
        << std::endl;
    }

    /* Type Inference */
    StyioAnalyzer analyzer = StyioAnalyzer();
    analyzer.typeInfer(styio_ast);

    if (show_all or show_styio_ast) {
      std::cout
        << "\033[1;32mAST\033[0m \033[1;33m-After-Type-Checking\033[0m"
        << "\n"
        << styio_ast->toString(&styio_repr) << "\n"
        << std::endl;
    }

    /* Generate Styio IR */
    StyioIR* styio_ir = analyzer.toStyioIR(styio_ast);

    if (show_all or show_styio_ir) {
      std::cout
        << "\033[1;32mSTYIO IR\033[0m \033[1;33m\033[0m"
        << "\n"
        << styio_ir->toString(&styio_repr) << "\n"
        << std::endl;
    }

    /* JIT Initialization */
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    llvm::ExitOnError exit_on_error;
    std::unique_ptr<StyioJIT_ORC> styio_orc_jit = exit_on_error(StyioJIT_ORC::Create());

    /* CodeGen Initialization */
    StyioToLLVM generator = StyioToLLVM(std::move(styio_orc_jit));

    /* CodeGen (LLVM IR) */
    styio_ir->toLLVMIR(&generator);

    if (show_llvm_ir) {
      generator.print_llvm_ir();
    }

    /* JIT Execute */
    generator.execute();
  }

  return 0;
}