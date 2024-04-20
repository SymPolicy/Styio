#ifndef STYIO_LLVM_EXECUTIONENGINE_ORC_JIT_H
#define STYIO_LLVM_EXECUTIONENGINE_ORC_JIT_H

#include <memory>

#include "../StyioExtern/ExternLib.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"

class StyioJIT_ORC
{
private:
  std::unique_ptr<llvm::orc::ExecutionSession> ES;

  llvm::DataLayout DL;
  llvm::orc::MangleAndInterner Mangle;

  llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
  llvm::orc::IRCompileLayer CompileLayer;

  llvm::orc::JITDylib &MainJD;

public:
  StyioJIT_ORC(
    std::unique_ptr<llvm::orc::ExecutionSession> ES,
    llvm::orc::JITTargetMachineBuilder JTMB,
    llvm::DataLayout DL
  ) :
      ES(std::move(ES)),
      DL(std::move(DL)),
      Mangle(*this->ES, this->DL),
      ObjectLayer(*this->ES, []()
                  {
                    return std::make_unique<llvm::SectionMemoryManager>();
                  }),
      CompileLayer(*this->ES, ObjectLayer, std::make_unique<llvm::orc::ConcurrentIRCompiler>(std::move(JTMB))),
      MainJD(this->ES->createBareJITDylib("<main>")) {
    MainJD.addGenerator(llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix())));

    // llvm::orc::SymbolMap symbolMap;
    // // Register every symbol that can be accessed from the JIT'ed code.
    // symbolMap[Mangle("jitExample")] = llvm::orc::ExecutorSymbolDef(
    //     llvm::pointerToJITTargetAddress(&jitExample), llvm::JITSymbolFlags());

    // llvm::cantFail(MainJD.define(absoluteSymbols(symbolMap)));

    // MainJD.define(
    //   llvm::orc::absoluteSymbols({
    //     { Mangle("puts"), llvm::orc::ExecutorAddr::fromPtr(&puts)},
    //     { Mangle("printf"), llvm::orc::ExecutorAddr::fromPtr(&printf)}
    // }));

    if (auto DLSGOrErr =
        llvm::orc::DynamicLibrarySearchGenerator::Load("/root/Styio/src/include/StyioJIT/ExternLib.hpp",
                                            DL.getGlobalPrefix()))
      MainJD.addGenerator(std::move(*DLSGOrErr));

    // MainJD.setGenerator(llvm::orc::DynamicLibrarySearchGenerator::Load(
    //   "/root/Styio/src/include/StyioJIT/ExternLib.hpp", DL.getGlobalPrefix()));

    // MainJD.addGenerator(llvm::orc::DynamicLibrarySearchGenerator::Load(
    //   "/root/Styio/src/include/StyioJIT/ExternLib.hpp", DL.getGlobalPrefix()))

    if (JTMB.getTargetTriple().isOSBinFormatCOFF()) {
      ObjectLayer.setOverrideObjectFlagsWithResponsibilityFlags(true);
      ObjectLayer.setAutoClaimResponsibilityForObjectSymbols(true);
    }
  }

  ~StyioJIT_ORC() {
    if (auto Err = ES->endSession())
      ES->reportError(std::move(Err));
  }

  static llvm::Expected<std::unique_ptr<StyioJIT_ORC>> Create() {
    auto EPC = llvm::orc::SelfExecutorProcessControl::Create();
    if (!EPC)
      return EPC.takeError();

    auto ES = std::make_unique<llvm::orc::ExecutionSession>(std::move(*EPC));

    llvm::orc::JITTargetMachineBuilder JTMB(ES->getExecutorProcessControl().getTargetTriple());

    auto DL = JTMB.getDefaultDataLayoutForTarget();
    if (!DL)
      return DL.takeError();

    return std::make_unique<StyioJIT_ORC>(std::move(ES), std::move(JTMB), std::move(*DL));
  }

  const llvm::DataLayout &getDataLayout() const {
    return DL;
  }

  llvm::orc::JITDylib &getMainJITDylib() {
    return MainJD;
  }

  llvm::Error addModule(llvm::orc::ThreadSafeModule TSM, llvm::orc::ResourceTrackerSP RT = nullptr) {
    if (!RT)
      RT = MainJD.getDefaultResourceTracker();
    return CompileLayer.add(RT, std::move(TSM));
  }

  llvm::Expected<llvm::orc::ExecutorSymbolDef> lookup(llvm::StringRef Name) {
    return ES->lookup({&MainJD}, Mangle(Name.str()));
  }
};

#endif  // STYIO_LLVM_EXECUTIONENGINE_ORC_JIT