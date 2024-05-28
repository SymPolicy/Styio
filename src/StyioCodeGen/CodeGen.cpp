// [C++ STL]
#include <iostream>
#include <string>

// [Styio]
#include "CodeGenVisitor.hpp"
#include "llvm/Support/raw_ostream.h"

void
StyioToLLVM::print_llvm_ir() {
  std::cout << "\033[1;32mLLVM IR\033[0m" << std::endl;

  /* llvm ir -> stdout */
  theModule->print(llvm::outs(), nullptr);
  /* llvm ir -> stderr */
  // llvm_module -> print(llvm::errs(), nullptr);

  std::cout << std::endl;
}

void
StyioToLLVM::execute() {
  std::cout << "\033[1;32mJIT\033[0m" << std::endl;

  auto RT = theORCJIT->getMainJITDylib().createResourceTracker();
  auto TSM = llvm::orc::ThreadSafeModule(std::move(theModule), std::move(theContext));
  llvm::ExitOnError exit_on_error;
  exit_on_error(theORCJIT->addModule(std::move(TSM), RT));

  // Look up the JIT'd code entry point.
  auto ExprSymbol = theORCJIT->lookup("main");
  if (!ExprSymbol) {
    std::cout << "main not found" << std::endl;
    return;
  } 
  else {
    std::cout << "main found" << std::endl;
  }

  // Cast the entry point address to a function pointer.
  int (*FP)() = ExprSymbol->getAddress().toPtr<int (*)()>();

  // Call into JIT'd code.
  std::cout << "result: " << FP() << std::endl;
}
