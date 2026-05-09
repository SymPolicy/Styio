// [C++ STL]
#include <iostream>
#include <string>

// [Styio]
#include "CodeGenVisitor.hpp"
#include "StyioException/Exception.hpp"
#include "StyioUtil/Util.hpp"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

void
StyioToLLVM::print_llvm_ir() {
  /* Use the same LLVM stream as Module::print so banner + IR stay ordered (cout may be buffered separately). */
  if (styio_stdout_plain()) {
    llvm::outs() << "LLVM IR\n";
  }
  else {
    llvm::outs() << "\033[1;32mLLVM IR\033[0m\n";
  }

  theModule->print(llvm::outs(), nullptr);
  llvm::outs() << "\n";
  llvm::outs().flush();
}

void
StyioToLLVM::execute() {
  std::string verifier_error;
  llvm::raw_string_ostream verifier_stream(verifier_error);
  if (llvm::verifyModule(*theModule, &verifier_stream)) {
    verifier_stream.flush();
    throw StyioTypeError("LLVM module verification failed: " + verifier_error);
  }
  auto RT = theORCJIT->getMainJITDylib().createResourceTracker();
  auto TSM = llvm::orc::ThreadSafeModule(std::move(theModule), std::move(theContext));
  llvm::ExitOnError exit_on_error;
  exit_on_error(theORCJIT->addModule(std::move(TSM), RT));

  auto ExprSymbol = theORCJIT->lookup("main");
  if (!ExprSymbol) {
    std::cerr << "styio: main not found" << std::endl;
    return;
  }

  int (*FP)() = ExprSymbol->getAddress().toPtr<int (*)()>();
  FP();
}

std::string
StyioToLLVM::dump_llvm_ir() const {
  std::string out;
  llvm::raw_string_ostream os(out);
  theModule->print(os, nullptr);
  os.flush();
  return out;
}
