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

    llvm::orc::SymbolMap runtime_symbols;
    auto add_symbol = [&](const char* name, auto* fn)
    {
      runtime_symbols[Mangle(name)] = {
        llvm::orc::ExecutorAddr::fromPtr(fn),
        llvm::JITSymbolFlags::Callable,
      };
    };

    add_symbol("something", &something);
    add_symbol("styio_file_open", &styio_file_open);
    add_symbol("styio_file_open_auto", &styio_file_open_auto);
    add_symbol("styio_file_open_write", &styio_file_open_write);
    add_symbol("styio_file_close", &styio_file_close);
    add_symbol("styio_file_rewind", &styio_file_rewind);
    add_symbol("styio_file_read_line", &styio_file_read_line);
    add_symbol("styio_file_write_cstr", &styio_file_write_cstr);
    add_symbol("styio_cstr_to_i64", &styio_cstr_to_i64);
    add_symbol("styio_cstr_to_f64", &styio_cstr_to_f64);
    add_symbol("styio_read_file_i64line", &styio_read_file_i64line);
    add_symbol("styio_strcat_ab", &styio_strcat_ab);
    add_symbol("styio_free_cstr", &styio_free_cstr);
    add_symbol("styio_i64_dec_cstr", &styio_i64_dec_cstr);
    add_symbol("styio_f64_dec_cstr", &styio_f64_dec_cstr);
    add_symbol("styio_runtime_has_error", &styio_runtime_has_error);
    add_symbol("styio_runtime_last_error", &styio_runtime_last_error);
    add_symbol("styio_runtime_last_error_subcode", &styio_runtime_last_error_subcode);
    add_symbol("styio_runtime_clear_error", &styio_runtime_clear_error);
    add_symbol("styio_runtime_set_log_sink", &styio_runtime_set_log_sink);
    add_symbol("styio_stdout_write_cstr", &styio_stdout_write_cstr);
    add_symbol("styio_stderr_write_cstr", &styio_stderr_write_cstr);
    add_symbol("styio_stdin_read_line", &styio_stdin_read_line);

    add_symbol("styio_list_i64_read_stdin", &styio_list_i64_read_stdin);
    add_symbol("styio_list_cstr_read_stdin", &styio_list_cstr_read_stdin);
    add_symbol("styio_string_lines", &styio_string_lines);
    add_symbol("styio_list_new_bool", &styio_list_new_bool);
    add_symbol("styio_list_new_i64", &styio_list_new_i64);
    add_symbol("styio_list_new_f64", &styio_list_new_f64);
    add_symbol("styio_list_new_cstr", &styio_list_new_cstr);
    add_symbol("styio_list_new_list", &styio_list_new_list);
    add_symbol("styio_list_new_dict", &styio_list_new_dict);
    add_symbol("styio_list_push_bool", &styio_list_push_bool);
    add_symbol("styio_list_push_i64", &styio_list_push_i64);
    add_symbol("styio_list_push_f64", &styio_list_push_f64);
    add_symbol("styio_list_push_cstr", &styio_list_push_cstr);
    add_symbol("styio_list_push_list", &styio_list_push_list);
    add_symbol("styio_list_push_dict", &styio_list_push_dict);
    add_symbol("styio_list_insert_bool", &styio_list_insert_bool);
    add_symbol("styio_list_insert_i64", &styio_list_insert_i64);
    add_symbol("styio_list_insert_f64", &styio_list_insert_f64);
    add_symbol("styio_list_insert_cstr", &styio_list_insert_cstr);
    add_symbol("styio_list_insert_list", &styio_list_insert_list);
    add_symbol("styio_list_insert_dict", &styio_list_insert_dict);
    add_symbol("styio_list_clone", &styio_list_clone);
    add_symbol("styio_list_len", &styio_list_len);
    add_symbol("styio_list_get_bool", &styio_list_get_bool);
    add_symbol("styio_list_get", &styio_list_get);
    add_symbol("styio_list_get_f64", &styio_list_get_f64);
    add_symbol("styio_list_get_cstr", &styio_list_get_cstr);
    add_symbol("styio_list_get_list", &styio_list_get_list);
    add_symbol("styio_list_get_dict", &styio_list_get_dict);
    add_symbol("styio_list_set_bool", &styio_list_set_bool);
    add_symbol("styio_list_set", &styio_list_set);
    add_symbol("styio_list_set_f64", &styio_list_set_f64);
    add_symbol("styio_list_set_cstr", &styio_list_set_cstr);
    add_symbol("styio_list_set_list", &styio_list_set_list);
    add_symbol("styio_list_set_dict", &styio_list_set_dict);
    add_symbol("styio_list_pop", &styio_list_pop);
    add_symbol("styio_list_to_cstr", &styio_list_to_cstr);
    add_symbol("styio_list_release", &styio_list_release);
    add_symbol("styio_list_active_count", &styio_list_active_count);

    add_symbol("styio_dict_new_bool", &styio_dict_new_bool);
    add_symbol("styio_dict_new_i64", &styio_dict_new_i64);
    add_symbol("styio_dict_new_f64", &styio_dict_new_f64);
    add_symbol("styio_dict_new_cstr", &styio_dict_new_cstr);
    add_symbol("styio_dict_new_list", &styio_dict_new_list);
    add_symbol("styio_dict_new_dict", &styio_dict_new_dict);
    add_symbol("styio_dict_clone", &styio_dict_clone);
    add_symbol("styio_dict_len", &styio_dict_len);
    add_symbol("styio_dict_get_bool", &styio_dict_get_bool);
    add_symbol("styio_dict_get_i64", &styio_dict_get_i64);
    add_symbol("styio_dict_get_f64", &styio_dict_get_f64);
    add_symbol("styio_dict_get_cstr", &styio_dict_get_cstr);
    add_symbol("styio_dict_get_list", &styio_dict_get_list);
    add_symbol("styio_dict_get_dict", &styio_dict_get_dict);
    add_symbol("styio_dict_set_bool", &styio_dict_set_bool);
    add_symbol("styio_dict_set_i64", &styio_dict_set_i64);
    add_symbol("styio_dict_set_f64", &styio_dict_set_f64);
    add_symbol("styio_dict_set_cstr", &styio_dict_set_cstr);
    add_symbol("styio_dict_set_list", &styio_dict_set_list);
    add_symbol("styio_dict_set_dict", &styio_dict_set_dict);
    add_symbol("styio_dict_keys", &styio_dict_keys);
    add_symbol("styio_dict_values_bool", &styio_dict_values_bool);
    add_symbol("styio_dict_values_i64", &styio_dict_values_i64);
    add_symbol("styio_dict_values_f64", &styio_dict_values_f64);
    add_symbol("styio_dict_values_cstr", &styio_dict_values_cstr);
    add_symbol("styio_dict_values_list", &styio_dict_values_list);
    add_symbol("styio_dict_values_dict", &styio_dict_values_dict);
    add_symbol("styio_dict_to_cstr", &styio_dict_to_cstr);
    add_symbol("styio_dict_release", &styio_dict_release);
    add_symbol("styio_dict_active_count", &styio_dict_active_count);
    add_symbol("styio_dict_runtime_supported_impl_count", &styio_dict_runtime_supported_impl_count);
    add_symbol("styio_dict_runtime_supported_impl_name", &styio_dict_runtime_supported_impl_name);
    add_symbol("styio_dict_runtime_canonical_impl_name", &styio_dict_runtime_canonical_impl_name);
    add_symbol("styio_dict_runtime_set_impl_by_name", &styio_dict_runtime_set_impl_by_name);
    add_symbol("styio_dict_runtime_get_impl_name", &styio_dict_runtime_get_impl_name);
    add_symbol("styio_dict_runtime_set_impl", &styio_dict_runtime_set_impl);
    add_symbol("styio_dict_runtime_get_impl", &styio_dict_runtime_get_impl);

    auto err = MainJD.define(llvm::orc::absoluteSymbols(std::move(runtime_symbols)));

    // llvm::DenseSet<llvm::orc::SymbolStringPtr> AllowList({
    //   Mangle("something")
    // });

    // MainJD.addGenerator(llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
    //   DL.getGlobalPrefix(),
    //   [&](const llvm::orc::SymbolStringPtr &S) { return AllowList.count(S); })));

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

  llvm::Error defineAbsoluteSymbol(llvm::StringRef Name, void* address) {
    llvm::orc::SymbolMap symbols;
    symbols[Mangle(Name)] = {
      llvm::orc::ExecutorAddr::fromPtr(address),
      llvm::JITSymbolFlags::Callable,
    };
    return MainJD.define(llvm::orc::absoluteSymbols(std::move(symbols)));
  }

  llvm::Expected<llvm::orc::ExecutorSymbolDef> lookup(llvm::StringRef Name) {
    return ES->lookup({&MainJD}, Mangle(Name.str()));
  }
};

#endif  // STYIO_LLVM_EXECUTIONENGINE_ORC_JIT
