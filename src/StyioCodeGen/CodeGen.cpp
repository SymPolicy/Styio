// [C++ STL]
#include <iostream>
#include <string>

// [Styio]
#include "CodeGenVisitor.hpp"
#include "llvm/Support/raw_ostream.h"

void
StyioToLLVM::print_llvm_ir() {
  std::cout << "\n"
            << "\033[1;32mLLVM IR\033[0m"
            << "\n"
            << std::endl;

  /* llvm ir -> stdout */
  theModule->print(llvm::outs(), nullptr);
  /* llvm ir -> stderr */
  // llvm_module -> print(llvm::errs(), nullptr);
}