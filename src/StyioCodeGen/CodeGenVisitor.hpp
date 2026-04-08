#pragma once
#ifndef STYIO_CODE_GEN_VISITOR_H_
#define STYIO_CODE_GEN_VISITOR_H_

// [STL]
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// [Styio]
#include "../StyioIR/IRDecl.hpp"
#include "../StyioJIT/StyioJIT_ORC.hpp"

// [LLVM]
#include "llvm/Analysis/CGSCCPassManager.h" /* CGSCCAnalysisManager */
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h" /* FunctionPassManager */
#include "llvm/IR/Module.h"
#include "llvm/IR/PassInstrumentation.h" /* PassInstrumentationCallbacks */
#include "llvm/IR/PassManager.h"         /* LoopAnalysisManager, FunctionAnalysisManager, ModuleAnalysisManager */
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"                 /* PassBuilder */
#include "llvm/Passes/StandardInstrumentations.h"    /* StandardInstrumentations.h */
#include "llvm/Support/TargetSelect.h"               /* InitializeNativeTarget, InitializeNativeTargetAsmPrinter, InitializeNativeTargetAsmParser */
#include "llvm/Transforms/InstCombine/InstCombine.h" /* InstCombinePass */
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"         /* GVNPass */
#include "llvm/Transforms/Scalar/Reassociate.h" /* ReassociatePass */
#include "llvm/Transforms/Scalar/SimplifyCFG.h" /* SimplifyCFGPass */
#include "llvm/Transforms/Utils.h"

using std::string;
using std::unordered_map;
using std::vector;

struct SGPulsePlan;

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

// Generic Visitor
template <typename... Types>
class CodeGenVisitor;

template <typename T>
class CodeGenVisitor<T>
{
public:
  /* Get LLVM Type */
  virtual llvm::Type* toLLVMType(T* t) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(T* t) = 0;
};

template <typename T, typename... Types>
class CodeGenVisitor<T, Types...> : public CodeGenVisitor<Types...>
{
public:
  using CodeGenVisitor<Types...>::toLLVMType;
  using CodeGenVisitor<Types...>::toLLVMIR;

  /* Get LLVM Type */
  virtual llvm::Type* toLLVMType(T* t) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(T* t) = 0;
};

using StyioCodeGenVisitor = CodeGenVisitor<
  class SGResId,
  class SGType,

  class SGConstBool,

  class SGConstInt,
  class SGConstFloat,

  class SGConstChar,
  class SGConstString,
  class SGFormatString,

  class SGStruct,

  class SGCast,

  class SGBinOp,
  class SGCond,

  class SGVar,
  class SGFlexBind,
  class SGFinalBind,

  class SGFuncArg,
  class SGFunc,
  class SGCall,

  class SGReturn,

  class SGLoop,
  class SGForEach,
  class SGListLiteral,
  class SGStateSnapLoad,
  class SGStateHistLoad,
  class SGSeriesAvgStep,
  class SGSeriesMaxStep,
  class SGMatch,
  class SGBreak,
  class SGContinue,

  class SGUndef,
  class SGFallback,
  class SGWaveMerge,
  class SGWaveDispatch,
  class SGGuardSelect,
  class SGEqProbe,

  class SGHandleAcquire,
  class SGFileLineIter,
  class SGStreamZip,
  class SGSnapshotDecl,
  class SGSnapshotShadowLoad,
  class SGInstantPull,
  class SGResourceWriteToFile,
  class SIOStdStreamWrite,
  class SIOStdStreamLineIter,
  class SIOStdStreamPull,

  class SGBlock,
  class SGEntry,
  class SGMainEntry,

  class SIOPath,
  class SIOPrint,
  class SIORead>;

class StyioToLLVM : public StyioCodeGenVisitor
{
  unique_ptr<llvm::LLVMContext> theContext;
  unique_ptr<llvm::Module> theModule;
  unique_ptr<llvm::IRBuilder<>> theBuilder;

  std::unique_ptr<StyioJIT_ORC> theORCJIT;

  unique_ptr<llvm::FunctionPassManager> theFPM;
  unique_ptr<llvm::LoopAnalysisManager> theLAM;
  unique_ptr<llvm::FunctionAnalysisManager> theFAM;
  unique_ptr<llvm::CGSCCAnalysisManager> theCGAM;
  unique_ptr<llvm::ModuleAnalysisManager> theMAM;
  unique_ptr<llvm::PassInstrumentationCallbacks> thePIC;
  unique_ptr<llvm::StandardInstrumentations> theSI;
  llvm::PassBuilder thePB;

  unordered_map<string, llvm::AllocaInst*> mutable_variables; /* [FlexBind] Mutable Variables */
  unordered_map<string, llvm::Value*> named_values;  /* [FinalBind] Named Values = Immutable Variables */

  /* [|n|] final-bound rings: array alloca in mutable_variables + head cursor (next write index). */
  unordered_map<string, llvm::AllocaInst*> bounded_ring_head_slot_;
  unordered_map<string, std::uint64_t> bounded_ring_capacity_;

  struct LoopFrame {
    llvm::BasicBlock* break_dest = nullptr;
    llvm::BasicBlock* continue_dest = nullptr;
  };
  std::vector<LoopFrame> loop_stack_;

public:
  StyioToLLVM(std::unique_ptr<StyioJIT_ORC> styio_jit) :
      theContext(std::make_unique<llvm::LLVMContext>()),
      theModule(std::make_unique<llvm::Module>("styio", *theContext)),
      theBuilder(std::make_unique<llvm::IRBuilder<>>(*theContext)),
      theORCJIT(std::move(styio_jit)),
      theFPM(std::make_unique<llvm::FunctionPassManager>()),
      theLAM(std::make_unique<llvm::LoopAnalysisManager>()),
      theFAM(std::make_unique<llvm::FunctionAnalysisManager>()),
      theCGAM(std::make_unique<llvm::CGSCCAnalysisManager>()),
      theMAM(std::make_unique<llvm::ModuleAnalysisManager>()),
      thePIC(std::make_unique<llvm::PassInstrumentationCallbacks>()),
      theSI(std::make_unique<llvm::StandardInstrumentations>(*theContext, /*DebugLogging*/ true)) {
    theModule->setDataLayout(theORCJIT->getDataLayout());

    theSI->registerCallbacks(*thePIC, theMAM.get());

    // Add transform passes.
    // Do simple "peephole" optimizations and bit-twiddling optimizations.
    theFPM->addPass(llvm::InstCombinePass());
    // Reassociate expressions.
    theFPM->addPass(llvm::ReassociatePass());
    // Eliminate common sub-expressions.
    theFPM->addPass(llvm::GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    theFPM->addPass(llvm::SimplifyCFGPass());

    thePB.registerModuleAnalyses(*theMAM);
    thePB.registerFunctionAnalyses(*theFAM);
    thePB.crossRegisterProxies(*theLAM, *theFAM, *theCGAM, *theMAM);
  }

  ~StyioToLLVM() {}

  static StyioToLLVM* Create(std::unique_ptr<StyioJIT_ORC> styio_jit) {
    return new StyioToLLVM(std::move(styio_jit));
  }

  void print_llvm_ir();
  void execute();

  /** Module IR without ANSI or extra banners (for golden tests). */
  std::string dump_llvm_ir() const;

  /* CodeGen Get LLVM Type */
  llvm::Type* toLLVMType(SGResId* node);
  llvm::Type* toLLVMType(SGType* node);

  llvm::Type* toLLVMType(SGConstBool* node);

  llvm::Type* toLLVMType(SGConstInt* node);
  llvm::Type* toLLVMType(SGConstFloat* node);

  llvm::Type* toLLVMType(SGConstChar* node);
  llvm::Type* toLLVMType(SGConstString* node);
  llvm::Type* toLLVMType(SGFormatString* node);

  llvm::Type* toLLVMType(SGStruct* node);

  llvm::Type* toLLVMType(SGCast* node);

  llvm::Type* toLLVMType(SGBinOp* node);
  llvm::Type* toLLVMType(SGCond* node);

  llvm::Type* toLLVMType(SGVar* node);
  llvm::Type* toLLVMType(SGFlexBind* node);
  llvm::Type* toLLVMType(SGFinalBind* node);

  llvm::Type* toLLVMType(SGFuncArg* node);
  llvm::Type* toLLVMType(SGFunc* node);
  llvm::Type* toLLVMType(SGCall* node);

  llvm::Type* toLLVMType(SGReturn* node);

  llvm::Type* toLLVMType(SGLoop* node);
  llvm::Type* toLLVMType(SGForEach* node);
  llvm::Type* toLLVMType(SGListLiteral* node);
  llvm::Type* toLLVMType(SGStateSnapLoad* node);
  llvm::Type* toLLVMType(SGStateHistLoad* node);
  llvm::Type* toLLVMType(SGSeriesAvgStep* node);
  llvm::Type* toLLVMType(SGSeriesMaxStep* node);
  llvm::Type* toLLVMType(SGMatch* node);
  llvm::Type* toLLVMType(SGBreak* node);
  llvm::Type* toLLVMType(SGContinue* node);

  llvm::Type* toLLVMType(SGUndef* node);
  llvm::Type* toLLVMType(SGFallback* node);
  llvm::Type* toLLVMType(SGWaveMerge* node);
  llvm::Type* toLLVMType(SGWaveDispatch* node);
  llvm::Type* toLLVMType(SGGuardSelect* node);
  llvm::Type* toLLVMType(SGEqProbe* node);

  llvm::Type* toLLVMType(SGHandleAcquire* node);
  llvm::Type* toLLVMType(SGFileLineIter* node);
  llvm::Type* toLLVMType(SGStreamZip* node);
  llvm::Type* toLLVMType(SGSnapshotDecl* node);
  llvm::Type* toLLVMType(SGSnapshotShadowLoad* node);
  llvm::Type* toLLVMType(SGInstantPull* node);
  llvm::Type* toLLVMType(SGResourceWriteToFile* node);
  llvm::Type* toLLVMType(SIOStdStreamWrite* node);
  llvm::Type* toLLVMType(SIOStdStreamLineIter* node);
  llvm::Type* toLLVMType(SIOStdStreamPull* node);

  // llvm::Type* toLLVMType(SGIfElse* node);
  // llvm::Type* toLLVMType(SGForLoop* node);
  // llvm::Type* toLLVMType(SGWhileLoop* node);

  llvm::Type* toLLVMType(SGBlock* node);
  llvm::Type* toLLVMType(SGEntry* node);
  llvm::Type* toLLVMType(SGMainEntry* node);

  llvm::Type* toLLVMType(SIOPath* node);
  llvm::Type* toLLVMType(SIOPrint* node);
  llvm::Type* toLLVMType(SIORead* node);

  /* LLVM Code Generation */
  llvm::Value* toLLVMIR(SGResId* node);
  llvm::Value* toLLVMIR(SGType* node);

  llvm::Value* toLLVMIR(SGConstBool* node);

  llvm::Value* toLLVMIR(SGConstInt* node);
  llvm::Value* toLLVMIR(SGConstFloat* node);

  llvm::Value* toLLVMIR(SGConstChar* node);
  llvm::Value* toLLVMIR(SGConstString* node);
  llvm::Value* toLLVMIR(SGFormatString* node);

  llvm::Value* toLLVMIR(SGStruct* node);

  llvm::Value* toLLVMIR(SGCast* node);

  llvm::Value* toLLVMIR(SGBinOp* node);
  llvm::Value* toLLVMIR(SGCond* node);

  llvm::Value* toLLVMIR(SGVar* node);
  llvm::Value* toLLVMIR(SGFlexBind* node);
  llvm::Value* toLLVMIR(SGFinalBind* node);

  llvm::Value* toLLVMIR(SGFuncArg* node);
  llvm::Value* toLLVMIR(SGFunc* node);
  llvm::Value* toLLVMIR(SGCall* node);

  llvm::Value* toLLVMIR(SGReturn* node);

  llvm::Value* toLLVMIR(SGLoop* node);
  llvm::Value* toLLVMIR(SGForEach* node);
  llvm::Value* toLLVMIR(SGListLiteral* node);
  llvm::Value* toLLVMIR(SGStateSnapLoad* node);
  llvm::Value* toLLVMIR(SGStateHistLoad* node);
  llvm::Value* toLLVMIR(SGSeriesAvgStep* node);
  llvm::Value* toLLVMIR(SGSeriesMaxStep* node);
  llvm::Value* toLLVMIR(SGMatch* node);
  llvm::Value* toLLVMIR(SGBreak* node);
  llvm::Value* toLLVMIR(SGContinue* node);

  llvm::Value* toLLVMIR(SGUndef* node);
  llvm::Value* toLLVMIR(SGFallback* node);
  llvm::Value* toLLVMIR(SGWaveMerge* node);
  llvm::Value* toLLVMIR(SGWaveDispatch* node);
  llvm::Value* toLLVMIR(SGGuardSelect* node);
  llvm::Value* toLLVMIR(SGEqProbe* node);

  llvm::Value* toLLVMIR(SGHandleAcquire* node);
  llvm::Value* toLLVMIR(SGFileLineIter* node);
  llvm::Value* toLLVMIR(SGStreamZip* node);
  llvm::Value* toLLVMIR(SGSnapshotDecl* node);
  llvm::Value* toLLVMIR(SGSnapshotShadowLoad* node);
  llvm::Value* toLLVMIR(SGInstantPull* node);
  llvm::Value* toLLVMIR(SGResourceWriteToFile* node);
  llvm::Value* toLLVMIR(SIOStdStreamWrite* node);
  llvm::Value* toLLVMIR(SIOStdStreamLineIter* node);
  llvm::Value* toLLVMIR(SIOStdStreamPull* node);

  // llvm::Value* toLLVMIR(SGIfElse* node);
  // llvm::Value* toLLVMIR(SGForLoop* node);
  // llvm::Value* toLLVMIR(SGWhileLoop* node);

  llvm::Value* toLLVMIR(SGBlock* node);
  llvm::Value* toLLVMIR(SGEntry* node);
  llvm::Value* toLLVMIR(SGMainEntry* node);

  llvm::Value* toLLVMIR(SIOPath* node);
  llvm::Value* toLLVMIR(SIOPrint* node);
  llvm::Value* toLLVMIR(SIORead* node);

private:
  void declare_sgfunc(SGFunc* node);
  void define_sgfunc_body(SGFunc* node);
  static void collect_sgfuncs_postorder(SGFunc* node, std::vector<SGFunc*>& out);
  llvm::Value* coerce_for_return(llvm::Value* v, llvm::Type* want_ty);
  llvm::Value* truncate_for_main_ret(llvm::Value* v);

  llvm::Value* promote_to_cstr(llvm::Value* v);
  llvm::Value* evaluate_arm_block_value(SGBlock* b, bool mixed_phi);

  std::vector<std::vector<std::string>> file_handle_scope_stack_;
  std::vector<std::vector<llvm::AllocaInst*>> cstr_slot_scope_stack_;

  std::vector<std::pair<std::string, StyioIR*>> snapshot_path_exprs_;
  std::unordered_map<std::string, llvm::AllocaInst*> file_singleton_path_slots_;
  std::unordered_set<std::string> file_singleton_raii_paths_;
  std::unordered_set<llvm::Value*> owned_cstr_temps_;

  void emit_snapshot_shadow_reload();

  void push_file_handle_scope();

  void pop_file_handle_scope();

  void register_file_handle_for_raii(const std::string& var_name);
  void register_cstr_slot_for_raii(llvm::AllocaInst* slot);

  llvm::FunctionCallee free_cstr_fn();
  void track_owned_cstr_temp(llvm::Value* v);
  bool take_owned_cstr_temp(llvm::Value* v);
  void forget_owned_cstr_temp(llvm::Value* v);
  void free_cstr_if_runtime_owned(llvm::Value* v);
  void free_owned_cstr_temp_if_tracked(llvm::Value* v);

  llvm::Value* pulse_ledger_base_ = nullptr;
  llvm::Value* pulse_snap_base_ = nullptr;
  const SGPulsePlan* pulse_active_plan_ = nullptr;
  std::unordered_map<int, std::pair<llvm::Value*, const SGPulsePlan*>>
    pulse_region_ledgers_;

  llvm::Value* styio_load_i64_at_byte_ptr(llvm::Value* base, int byte_off);
  void styio_store_i64_at_byte_ptr(llvm::Value* base, int byte_off, llvm::Value* v);
  void pulse_copy_ledger_to_snap(llvm::Value* ledger, llvm::Value* snap, int nbytes);
  llvm::Value* coerce_pulse_input_i64(llvm::Value* v);
  void emit_pulse_commit_all(llvm::Value* ledger, const SGPulsePlan* plan);
};

#endif
