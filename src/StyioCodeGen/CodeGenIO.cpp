// [C++ STL]
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

// [Styio]
#include "../StyioIR/IRDecl.hpp"
#include "../StyioIR/StyioIR.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioIR/IOIR/IOIR.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "CodeGenVisitor.hpp"
#include "../StyioUtil/Util.hpp"

// [LLVM]
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/LinkAllIR.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils.h"

namespace {

constexpr std::int64_t STYIO_DYN_BOOL = 1;
constexpr std::int64_t STYIO_DYN_I64 = 2;
constexpr std::int64_t STYIO_DYN_F64 = 3;
constexpr std::int64_t STYIO_DYN_CSTR = 4;

bool
styio_task_result_is_f64(const StyioDataType& type) {
  return type.option == StyioDataTypeOption::Float;
}

bool
styio_task_result_is_cstr(const StyioDataType& type) {
  return type.option == StyioDataTypeOption::String;
}

void collect_task_free_refs(
  StyioIR* node,
  std::unordered_set<std::string>& refs,
  std::unordered_set<std::string>& locals);

void collect_task_block_refs(
  SGBlock* block,
  std::unordered_set<std::string>& refs,
  std::unordered_set<std::string> locals) {
  if (block == nullptr) {
    return;
  }
  for (StyioIR* stmt : block->stmts) {
    if (auto* bind = dynamic_cast<SGFlexBind*>(stmt)) {
      collect_task_free_refs(bind->value, refs, locals);
      if (bind->var != nullptr && bind->var->var_name != nullptr) {
        locals.insert(bind->var->var_name->as_str());
      }
      continue;
    }
    if (auto* bind = dynamic_cast<SGFinalBind*>(stmt)) {
      collect_task_free_refs(bind->value, refs, locals);
      if (bind->var != nullptr && bind->var->var_name != nullptr) {
        locals.insert(bind->var->var_name->as_str());
      }
      continue;
    }
    collect_task_free_refs(stmt, refs, locals);
  }
}

void collect_task_free_refs(
  StyioIR* node,
  std::unordered_set<std::string>& refs,
  std::unordered_set<std::string>& locals) {
  if (node == nullptr) {
    return;
  }
  if (auto* id = dynamic_cast<SGResId*>(node)) {
    const std::string name = id->as_str();
    if (locals.count(name) == 0) {
      refs.insert(name);
    }
    return;
  }
  if (auto* var = dynamic_cast<SGVar*>(node)) {
    if (var->val_init != nullptr) {
      collect_task_free_refs(var->val_init, refs, locals);
    }
    else if (var->var_name != nullptr) {
      const std::string name = var->var_name->as_str();
      if (locals.count(name) == 0) {
        refs.insert(name);
      }
    }
    return;
  }
  if (auto* dyn = dynamic_cast<SGDynLoad*>(node)) {
    if (locals.count(dyn->var_name) == 0) {
      refs.insert(dyn->var_name);
    }
    return;
  }
  if (auto* bin = dynamic_cast<SGBinOp*>(node)) {
    collect_task_free_refs(bin->lhs_expr, refs, locals);
    collect_task_free_refs(bin->rhs_expr, refs, locals);
    return;
  }
  if (auto* cond = dynamic_cast<SGCond*>(node)) {
    collect_task_free_refs(cond->lhs_expr, refs, locals);
    collect_task_free_refs(cond->rhs_expr, refs, locals);
    return;
  }
  if (auto* ret = dynamic_cast<SGReturn*>(node)) {
    collect_task_free_refs(ret->expr, refs, locals);
    return;
  }
  if (auto* block = dynamic_cast<SGBlock*>(node)) {
    collect_task_block_refs(block, refs, locals);
    return;
  }
  if (auto* bind = dynamic_cast<SGFlexBind*>(node)) {
    collect_task_free_refs(bind->value, refs, locals);
    if (bind->var != nullptr && bind->var->var_name != nullptr) {
      locals.insert(bind->var->var_name->as_str());
    }
    return;
  }
  if (auto* bind = dynamic_cast<SGFinalBind*>(node)) {
    collect_task_free_refs(bind->value, refs, locals);
    if (bind->var != nullptr && bind->var->var_name != nullptr) {
      locals.insert(bind->var->var_name->as_str());
    }
    return;
  }
  if (auto* loop = dynamic_cast<SGLoop*>(node)) {
    collect_task_free_refs(loop->cond, refs, locals);
    collect_task_block_refs(loop->body, refs, locals);
    return;
  }
  if (auto* each = dynamic_cast<SGForEach*>(node)) {
    collect_task_free_refs(each->iterable, refs, locals);
    auto body_locals = locals;
    body_locals.insert(each->var);
    collect_task_block_refs(each->body, refs, std::move(body_locals));
    return;
  }
  if (auto* range = dynamic_cast<SGRangeFor*>(node)) {
    collect_task_free_refs(range->start, refs, locals);
    collect_task_free_refs(range->end, refs, locals);
    collect_task_free_refs(range->step, refs, locals);
    auto body_locals = locals;
    body_locals.insert(range->var);
    collect_task_block_refs(range->body, refs, std::move(body_locals));
    return;
  }
  if (auto* branch = dynamic_cast<SGIf*>(node)) {
    collect_task_free_refs(branch->cond, refs, locals);
    collect_task_block_refs(branch->then_block, refs, locals);
    collect_task_block_refs(branch->else_block, refs, locals);
    return;
  }
  if (auto* match = dynamic_cast<SGMatch*>(node)) {
    collect_task_free_refs(match->scrutinee, refs, locals);
    for (auto& arm : match->int_arms) {
      collect_task_block_refs(arm.second, refs, locals);
    }
    collect_task_block_refs(match->default_arm, refs, locals);
    return;
  }
  if (auto* flow = dynamic_cast<SIOFlowBind*>(node)) {
    collect_task_free_refs(flow->source_expr, refs, locals);
    if (locals.count(flow->target_name) == 0) {
      refs.insert(flow->target_name);
    }
    return;
  }
  if (auto* write = dynamic_cast<SIOStdStreamWrite*>(node)) {
    for (StyioIR* expr : write->exprs) {
      collect_task_free_refs(expr, refs, locals);
    }
    return;
  }
  if (auto* print = dynamic_cast<SIOPrint*>(node)) {
    for (StyioIR* expr : print->expr) {
      collect_task_free_refs(expr, refs, locals);
    }
    return;
  }
}

}  // namespace

llvm::Value*
StyioToLLVM::toLLVMIR(SIOPath* node) {
  auto output = theBuilder->getInt32(0);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOPrint* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee stdout_fn = theModule->getOrInsertFunction(
    "styio_stdout_write_cstr",
    llvm::FunctionType::get(theBuilder->getVoidTy(), {char_ptr}, false));
  llvm::FunctionCallee i64_cstr_fn = theModule->getOrInsertFunction(
    "styio_i64_dec_cstr",
    llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));
  llvm::FunctionCallee f64_cstr_fn = theModule->getOrInsertFunction(
    "styio_f64_dec_cstr",
    llvm::FunctionType::get(char_ptr, {theBuilder->getDoubleTy()}, false));

  for (StyioIR* part : node->expr) {
    llvm::Value* v = part->toLLVMIR(this);
    llvm::Value* cstr = nullptr;

    if (v->getType()->isIntegerTy(1)) {
      llvm::Value* tstr = theBuilder->CreateGlobalStringPtr("true", "styio_true_nl");
      llvm::Value* fstr = theBuilder->CreateGlobalStringPtr("false", "styio_false_nl");
      cstr = theBuilder->CreateSelect(v, tstr, fstr);
    }
    else if (v->getType()->isIntegerTy(32)) {
      llvm::Value* ext = theBuilder->CreateSExt(v, theBuilder->getInt64Ty());
      cstr = theBuilder->CreateCall(i64_cstr_fn, {ext});
    }
    else if (v->getType()->isIntegerTy(64)) {
      llvm::Value* sent = llvm::ConstantInt::get(
        theBuilder->getInt64Ty(),
        static_cast<uint64_t>(std::numeric_limits<int64_t>::min()),
        true);
      llvm::Value* isU = theBuilder->CreateICmpEQ(v, sent);
      llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
      llvm::BasicBlock* b_at = llvm::BasicBlock::Create(*theContext, "print_at", F);
      llvm::BasicBlock* b_num = llvm::BasicBlock::Create(*theContext, "print_i64", F);
      llvm::BasicBlock* b_done = llvm::BasicBlock::Create(*theContext, "print_done", F);
      theBuilder->CreateCondBr(isU, b_at, b_num);
      theBuilder->SetInsertPoint(b_at);
      llvm::Value* ats = theBuilder->CreateGlobalStringPtr("@", "styio_print_at");
      theBuilder->CreateCall(stdout_fn, {ats});
      theBuilder->CreateBr(b_done);
      theBuilder->SetInsertPoint(b_num);
      llvm::Value* num_cstr = theBuilder->CreateCall(i64_cstr_fn, {v});
      theBuilder->CreateCall(stdout_fn, {num_cstr});
      theBuilder->CreateBr(b_done);
      theBuilder->SetInsertPoint(b_done);
    }
    else if (v->getType()->isDoubleTy()) {
      cstr = theBuilder->CreateCall(f64_cstr_fn, {v});
    }
    else if (v->getType()->isPointerTy()) {
      cstr = v;
    }
    else {
      llvm::Value* as_i64 = theBuilder->CreatePtrToInt(v, theBuilder->getInt64Ty());
      cstr = theBuilder->CreateCall(i64_cstr_fn, {as_i64});
    }

    if (cstr != nullptr) {
      theBuilder->CreateCall(stdout_fn, {cstr});
      if (v->getType()->isPointerTy()) {
        free_owned_cstr_temp_if_tracked(v);
      }
    }
    else if (v->getType()->isPointerTy()) {
      free_owned_cstr_temp_if_tracked(v);
    }
  }

  return theBuilder->getInt32(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIORead* node) {
  auto output = theBuilder->getInt32(0);
  return output;
}

/* ── M9: SIOStdStreamWrite (stdout / stderr) ────────────────────── */

llvm::Value*
StyioToLLVM::toLLVMIR(SIOStdStreamWrite* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);

  if (node->stream == SIOStdStreamWrite::Stream::Stdout) {
    /* ---- STDOUT: replicate SIOPrint six-type-branch pattern ---- */
    llvm::FunctionCallee stdout_fn = theModule->getOrInsertFunction(
      "styio_stdout_write_cstr",
      llvm::FunctionType::get(theBuilder->getVoidTy(), {char_ptr}, false));
    llvm::FunctionCallee i64_cstr_fn = theModule->getOrInsertFunction(
      "styio_i64_dec_cstr",
      llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));
    llvm::FunctionCallee f64_cstr_fn = theModule->getOrInsertFunction(
      "styio_f64_dec_cstr",
      llvm::FunctionType::get(char_ptr, {theBuilder->getDoubleTy()}, false));

    for (StyioIR* part : node->exprs) {
      llvm::Value* v = part->toLLVMIR(this);
      llvm::Value* cstr = nullptr;

      if (v->getType()->isIntegerTy(1)) {
        llvm::Value* tstr = theBuilder->CreateGlobalStringPtr("true", "styio_true_nl");
        llvm::Value* fstr = theBuilder->CreateGlobalStringPtr("false", "styio_false_nl");
        cstr = theBuilder->CreateSelect(v, tstr, fstr);
      }
      else if (v->getType()->isIntegerTy(32)) {
        llvm::Value* ext = theBuilder->CreateSExt(v, theBuilder->getInt64Ty());
        cstr = theBuilder->CreateCall(i64_cstr_fn, {ext});
      }
      else if (v->getType()->isIntegerTy(64)) {
        llvm::Value* sent = llvm::ConstantInt::get(
          theBuilder->getInt64Ty(),
          static_cast<uint64_t>(std::numeric_limits<int64_t>::min()),
          true);
        llvm::Value* isU = theBuilder->CreateICmpEQ(v, sent);
        llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
        llvm::BasicBlock* b_at = llvm::BasicBlock::Create(*theContext, "print_at", F);
        llvm::BasicBlock* b_num = llvm::BasicBlock::Create(*theContext, "print_i64", F);
        llvm::BasicBlock* b_done = llvm::BasicBlock::Create(*theContext, "print_done", F);
        theBuilder->CreateCondBr(isU, b_at, b_num);
        theBuilder->SetInsertPoint(b_at);
        llvm::Value* ats = theBuilder->CreateGlobalStringPtr("@", "styio_print_at");
        theBuilder->CreateCall(stdout_fn, {ats});
        theBuilder->CreateBr(b_done);
        theBuilder->SetInsertPoint(b_num);
        llvm::Value* num_cstr = theBuilder->CreateCall(i64_cstr_fn, {v});
        theBuilder->CreateCall(stdout_fn, {num_cstr});
        theBuilder->CreateBr(b_done);
        theBuilder->SetInsertPoint(b_done);
      }
      else if (v->getType()->isDoubleTy()) {
        cstr = theBuilder->CreateCall(f64_cstr_fn, {v});
      }
      else if (v->getType()->isPointerTy()) {
        cstr = v;
      }
      else {
        llvm::Value* as_i64 = theBuilder->CreatePtrToInt(v, theBuilder->getInt64Ty());
        cstr = theBuilder->CreateCall(i64_cstr_fn, {as_i64});
      }

      if (cstr != nullptr) {
        theBuilder->CreateCall(stdout_fn, {cstr});
        if (v->getType()->isPointerTy()) {
          free_owned_cstr_temp_if_tracked(v);
        }
      }
      else if (v->getType()->isPointerTy()) {
        free_owned_cstr_temp_if_tracked(v);
      }
    }
  }
  else {
    /* ---- STDERR: convert to cstr then call styio_stderr_write_cstr ---- */
    llvm::FunctionCallee stderr_fn = theModule->getOrInsertFunction(
      "styio_stderr_write_cstr",
      llvm::FunctionType::get(theBuilder->getVoidTy(), {char_ptr}, false));

    llvm::FunctionCallee i64_cstr_fn = theModule->getOrInsertFunction(
      "styio_i64_dec_cstr",
      llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));

    llvm::FunctionCallee f64_cstr_fn = theModule->getOrInsertFunction(
      "styio_f64_dec_cstr",
      llvm::FunctionType::get(char_ptr, {theBuilder->getDoubleTy()}, false));

    for (StyioIR* part : node->exprs) {
      llvm::Value* v = part->toLLVMIR(this);
      llvm::Value* cstr = nullptr;

      if (v->getType()->isIntegerTy(1)) {
        llvm::Value* tstr = theBuilder->CreateGlobalStringPtr("true", "styio_err_true");
        llvm::Value* fstr = theBuilder->CreateGlobalStringPtr("false", "styio_err_false");
        cstr = theBuilder->CreateSelect(v, tstr, fstr);
      }
      else if (v->getType()->isIntegerTy(32)) {
        llvm::Value* ext = theBuilder->CreateSExt(v, theBuilder->getInt64Ty());
        cstr = theBuilder->CreateCall(i64_cstr_fn, {ext});
      }
      else if (v->getType()->isIntegerTy(64)) {
        /* i64: check undefined sentinel, then convert. */
        llvm::Value* sent = llvm::ConstantInt::get(
          theBuilder->getInt64Ty(),
          static_cast<uint64_t>(std::numeric_limits<int64_t>::min()),
          true);
        llvm::Value* isU = theBuilder->CreateICmpEQ(v, sent);
        llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
        llvm::BasicBlock* b_at = llvm::BasicBlock::Create(*theContext, "stderr_at", F);
        llvm::BasicBlock* b_num = llvm::BasicBlock::Create(*theContext, "stderr_i64", F);
        llvm::BasicBlock* b_done = llvm::BasicBlock::Create(*theContext, "stderr_i64_done", F);
        theBuilder->CreateCondBr(isU, b_at, b_num);

        theBuilder->SetInsertPoint(b_at);
        llvm::Value* at_str = theBuilder->CreateGlobalStringPtr("@", "styio_stderr_at");
        theBuilder->CreateCall(stderr_fn, {at_str});
        theBuilder->CreateBr(b_done);

        theBuilder->SetInsertPoint(b_num);
        llvm::Value* num_cstr = theBuilder->CreateCall(i64_cstr_fn, {v});
        theBuilder->CreateCall(stderr_fn, {num_cstr});
        theBuilder->CreateBr(b_done);

        theBuilder->SetInsertPoint(b_done);
        continue;
      }
      else if (v->getType()->isDoubleTy()) {
        cstr = theBuilder->CreateCall(f64_cstr_fn, {v});
      }
      else if (v->getType()->isPointerTy()) {
        cstr = v;
      }
      else {
        llvm::Value* as_i64 = theBuilder->CreatePtrToInt(v, theBuilder->getInt64Ty());
        cstr = theBuilder->CreateCall(i64_cstr_fn, {as_i64});
      }

      /* styio_stderr_write_cstr appends \n and flushes. */
      theBuilder->CreateCall(stderr_fn, {cstr});
      if (v->getType()->isPointerTy()) {
        free_owned_cstr_temp_if_tracked(v);
      }
    }
  }

  return theBuilder->getInt32(0);
}

/* ── M10: SIOStdStreamLineIter ──────────────────────────────────────── */

llvm::Value*
StyioToLLVM::toLLVMIR(SIOStdStreamLineIter* node) {
  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);

  llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
    "styio_stdin_read_line",
    llvm::FunctionType::get(char_ptr, {}, false));

  llvm::BasicBlock* hdr = llvm::BasicBlock::Create(*theContext, "stdin_hdr", F);
  llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "stdin_body", F);
  llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "stdin_exit", F);

  /* Pulse plan setup (same pattern as SIOFileLineIter). */
  llvm::AllocaInst* ledger_alloc = nullptr;
  llvm::AllocaInst* snap_alloc = nullptr;
  int pulse_sz = 0;
  if (node->pulse_plan && node->pulse_plan->total_bytes > 0) {
    pulse_sz = node->pulse_plan->total_bytes;
    llvm::ArrayType* paty =
      llvm::ArrayType::get(theBuilder->getInt8Ty(), static_cast<unsigned>(pulse_sz));
    ledger_alloc = theBuilder->CreateAlloca(paty, nullptr, "pulse_ledger_stdin");
    snap_alloc = theBuilder->CreateAlloca(paty, nullptr, "pulse_snap_stdin");
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    llvm::Value* si8 = theBuilder->CreateBitCast(snap_alloc, i8p);
    theBuilder->CreateMemSet(
      li8,
      llvm::ConstantInt::get(theBuilder->getInt8Ty(), 0),
      llvm::ConstantInt::get(theBuilder->getInt64Ty(), pulse_sz),
      llvm::MaybeAlign(8));
    theBuilder->CreateMemSet(
      si8,
      llvm::ConstantInt::get(theBuilder->getInt8Ty(), 0),
      llvm::ConstantInt::get(theBuilder->getInt64Ty(), pulse_sz),
      llvm::MaybeAlign(8));
  }

  theBuilder->CreateBr(hdr);
  theBuilder->SetInsertPoint(hdr);

  /* Read one line from stdin. */
  llvm::Value* lineptr = theBuilder->CreateCall(read_fn, {});
  llvm::Value* null_line = llvm::ConstantPointerNull::get(
    llvm::cast<llvm::PointerType>(char_ptr));
  llvm::Value* done = theBuilder->CreateICmpEQ(lineptr, null_line);
  theBuilder->CreateCondBr(done, exit_bb, body_bb);

  theBuilder->SetInsertPoint(body_bb);
  llvm::AllocaInst* line_slot = theBuilder->CreateAlloca(char_ptr, nullptr, node->line_var);
  theBuilder->CreateStore(lineptr, line_slot);
  mutable_variables[node->line_var] = line_slot;

  emit_snapshot_shadow_reload();

  if (pulse_sz > 0) {
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    llvm::Value* si8 = theBuilder->CreateBitCast(snap_alloc, i8p);
    pulse_copy_ledger_to_snap(li8, si8, pulse_sz);
    pulse_ledger_base_ = li8;
    pulse_snap_base_ = si8;
    pulse_active_plan_ = node->pulse_plan.get();
  }

  node->body->toLLVMIR(this);

  if (pulse_sz > 0) {
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    emit_pulse_commit_all(li8, node->pulse_plan.get());
    pulse_ledger_base_ = nullptr;
    pulse_snap_base_ = nullptr;
    pulse_active_plan_ = nullptr;
  }

  mutable_variables.erase(node->line_var);
  llvm::BasicBlock* b2 = theBuilder->GetInsertBlock();
  if (b2 && !b2->getTerminator()) {
    theBuilder->CreateBr(hdr);
  }

  theBuilder->SetInsertPoint(exit_bb);
  if (pulse_sz > 0 && node->pulse_region_id >= 0) {
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    pulse_region_ledgers_[node->pulse_region_id] = {li8, node->pulse_plan.get()};
  }
  return theBuilder->getInt64(0);
}

/* ── M10: SIOStdStreamPull (single-read) ─────────────────────────────── */

llvm::Value*
StyioToLLVM::toLLVMIR(SIOStdStreamPull* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);

  llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
    "styio_stdin_read_line",
    llvm::FunctionType::get(char_ptr, {}, false));

  llvm::Value* line = theBuilder->CreateCall(read_fn, {});
  /* Convert to i64 (matches InstantPullAST's data type convention). */
  return cstr_to_i64_checked(line);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOTaskCreate* node) {
  struct TaskCapture {
    std::string name;
    llvm::Type* type = nullptr;
    llvm::Value* value = nullptr;
    bool mutable_slot = false;
  };

  auto is_capture_type = [](llvm::Type* type) {
    return type != nullptr
      && (type->isIntegerTy() || type->isDoubleTy() || type->isPointerTy());
  };

  std::vector<TaskCapture> captures;
  std::unordered_set<std::string> capture_refs;
  std::unordered_set<std::string> task_locals;
  collect_task_block_refs(node->body, capture_refs, std::move(task_locals));

  captures.reserve(mutable_variables.size() + named_values.size());
  for (const auto& item : mutable_variables) {
    if (capture_refs.count(item.first) == 0) {
      continue;
    }
    llvm::AllocaInst* slot = item.second;
    if (slot == nullptr || dynamic_variable_names_.count(item.first) != 0
        || list_slot_names_.count(item.first) != 0) {
      continue;
    }
    llvm::Type* type = slot->getAllocatedType();
    if (!is_capture_type(type)) {
      continue;
    }
    captures.push_back(TaskCapture{
      item.first,
      type,
      theBuilder->CreateLoad(type, slot, item.first + ".capture"),
      true});
  }
  for (const auto& item : named_values) {
    if (capture_refs.count(item.first) == 0) {
      continue;
    }
    if (mutable_variables.count(item.first) != 0 || item.second == nullptr) {
      continue;
    }
    llvm::Type* type = item.second->getType();
    if (!is_capture_type(type)) {
      continue;
    }
    captures.push_back(TaskCapture{item.first, type, item.second, false});
  }
  std::sort(
    captures.begin(),
    captures.end(),
    [](const TaskCapture& lhs, const TaskCapture& rhs) {
      return lhs.name < rhs.name;
    });

  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::Type* fn_ret = theBuilder->getInt64Ty();
  if (styio_task_result_is_f64(node->result_type)) {
    fn_ret = theBuilder->getDoubleTy();
  }
  else if (styio_task_result_is_cstr(node->result_type)) {
    fn_ret = char_ptr;
  }

  std::vector<llvm::Type*> capture_types;
  capture_types.reserve(captures.size());
  for (const TaskCapture& cap : captures) {
    capture_types.push_back(cap.type);
  }
  llvm::StructType* ctx_ty = nullptr;
  llvm::Value* ctx_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::get(*theContext, 0));
  if (!capture_types.empty()) {
    ctx_ty = llvm::StructType::create(
      *theContext,
      capture_types,
      "styio_task_ctx." + std::to_string(task_function_counter_));
    const uint64_t ctx_bytes = theModule->getDataLayout().getTypeAllocSize(ctx_ty);
    llvm::FunctionCallee malloc_fn = theModule->getOrInsertFunction(
      "malloc",
      llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));
    ctx_ptr = theBuilder->CreateCall(malloc_fn, {theBuilder->getInt64(ctx_bytes)});
    for (std::size_t i = 0; i < captures.size(); ++i) {
      llvm::Value* field = theBuilder->CreateStructGEP(ctx_ty, ctx_ptr, static_cast<unsigned>(i));
      theBuilder->CreateStore(captures[i].value, field);
    }
  }

  const std::string task_name = "__styio_task_" + std::to_string(task_function_counter_++);
  llvm::FunctionType* task_fty = llvm::FunctionType::get(fn_ret, {char_ptr}, false);
  llvm::Function* task_fn = llvm::Function::Create(
    task_fty,
    llvm::Function::InternalLinkage,
    task_name,
    *theModule);
  task_fn->getArg(0)->setName("ctx");

  llvm::BasicBlock* caller_bb = theBuilder->GetInsertBlock();
  auto saved_mut = mutable_variables;
  auto saved_named = named_values;
  auto saved_ring_h = bounded_ring_head_slot_;
  auto saved_ring_c = bounded_ring_capacity_;
  auto saved_dyn_names = dynamic_variable_names_;
  auto saved_list_names = list_slot_names_;
  auto saved_file_scopes = file_handle_scope_stack_;
  auto saved_cstr_scopes = cstr_slot_scope_stack_;
  auto saved_dynamic_scopes = dynamic_slot_scope_stack_;
  auto saved_owned_cstr = owned_cstr_temps_;
  auto saved_owned_resource = owned_resource_temps_;

  mutable_variables.clear();
  named_values.clear();
  bounded_ring_head_slot_.clear();
  bounded_ring_capacity_.clear();
  dynamic_variable_names_.clear();
  list_slot_names_.clear();
  file_handle_scope_stack_.clear();
  cstr_slot_scope_stack_.clear();
  dynamic_slot_scope_stack_.clear();
  owned_cstr_temps_.clear();
  owned_resource_temps_.clear();

  llvm::BasicBlock* entry = llvm::BasicBlock::Create(*theContext, "task_entry", task_fn);
  theBuilder->SetInsertPoint(entry);
  llvm::Argument* ctx_arg = task_fn->getArg(0);
  for (std::size_t i = 0; i < captures.size(); ++i) {
    llvm::Value* field = theBuilder->CreateStructGEP(ctx_ty, ctx_arg, static_cast<unsigned>(i));
    llvm::Value* loaded = theBuilder->CreateLoad(captures[i].type, field, captures[i].name);
    if (captures[i].mutable_slot) {
      llvm::AllocaInst* slot = create_entry_alloca(captures[i].type, captures[i].name);
      theBuilder->CreateStore(loaded, slot);
      mutable_variables[captures[i].name] = slot;
    }
    else {
      named_values[captures[i].name] = loaded;
    }
  }

  push_file_handle_scope();
  llvm::Value* result = nullptr;
  if (node->body != nullptr) {
    for (StyioIR* stmt : node->body->stmts) {
      if (auto* ret = dynamic_cast<SGReturn*>(stmt)) {
        result = ret->expr != nullptr ? ret->expr->toLLVMIR(this) : nullptr;
        break;
      }
      result = stmt->toLLVMIR(this);
      if (theBuilder->GetInsertBlock()->getTerminator()) {
        break;
      }
    }
  }

  llvm::BasicBlock* task_cur = theBuilder->GetInsertBlock();
  if (task_cur != nullptr && !task_cur->getTerminator()) {
    if (result == nullptr) {
      if (fn_ret->isDoubleTy()) {
        result = llvm::ConstantFP::get(fn_ret, 0.0);
      }
      else if (fn_ret->isPointerTy()) {
        result = theBuilder->CreateGlobalStringPtr("", task_name + ".empty");
      }
      else {
        result = theBuilder->getInt64(0);
      }
    }
    else if (fn_ret->isDoubleTy() && result->getType()->isIntegerTy()) {
      result = theBuilder->CreateSIToFP(result, fn_ret);
    }
    else if (fn_ret->isIntegerTy(64) && result->getType()->isIntegerTy(1)) {
      result = theBuilder->CreateZExt(result, fn_ret);
    }
    else if (fn_ret->isIntegerTy(64) && result->getType()->isIntegerTy()
             && !result->getType()->isIntegerTy(64)) {
      result = theBuilder->CreateSExtOrTrunc(result, fn_ret);
    }
    else if (fn_ret->isIntegerTy(64) && result->getType()->isDoubleTy()) {
      result = theBuilder->CreateFPToSI(result, fn_ret);
    }
    else if (fn_ret->isIntegerTy(64) && result->getType()->isPointerTy()) {
      result = cstr_to_i64_checked(result);
    }
    else if (fn_ret->isPointerTy() && !result->getType()->isPointerTy()) {
      result = promote_to_cstr(result);
    }
    pop_file_handle_scope();
    theBuilder->CreateRet(result);
  }

  mutable_variables = std::move(saved_mut);
  named_values = std::move(saved_named);
  bounded_ring_head_slot_ = std::move(saved_ring_h);
  bounded_ring_capacity_ = std::move(saved_ring_c);
  dynamic_variable_names_ = std::move(saved_dyn_names);
  list_slot_names_ = std::move(saved_list_names);
  file_handle_scope_stack_ = std::move(saved_file_scopes);
  cstr_slot_scope_stack_ = std::move(saved_cstr_scopes);
  dynamic_slot_scope_stack_ = std::move(saved_dynamic_scopes);
  owned_cstr_temps_ = std::move(saved_owned_cstr);
  owned_resource_temps_ = std::move(saved_owned_resource);
  theBuilder->SetInsertPoint(caller_bb);

  llvm::FunctionCallee spawn_fn = nullptr;
  if (styio_task_result_is_f64(node->result_type)) {
    spawn_fn = theModule->getOrInsertFunction(
      "styio_task_f64_spawn",
      llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr, char_ptr}, false));
  }
  else if (styio_task_result_is_cstr(node->result_type)) {
    spawn_fn = theModule->getOrInsertFunction(
      "styio_task_cstr_spawn",
      llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr, char_ptr}, false));
  }
  else {
    spawn_fn = theModule->getOrInsertFunction(
      "styio_task_i64_spawn",
      llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr, char_ptr}, false));
  }
  return theBuilder->CreateCall(spawn_fn, {task_fn, ctx_ptr});
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOFlowBind* node) {
  llvm::Value* value = node->source_expr != nullptr
    ? node->source_expr->toLLVMIR(this)
    : theBuilder->getInt64(0);

  if (node->source_is_task) {
    if (styio_task_result_is_f64(node->result_type)) {
      llvm::FunctionCallee pull_fn = theModule->getOrInsertFunction(
        "styio_task_f64_pull",
        llvm::FunctionType::get(theBuilder->getDoubleTy(), {theBuilder->getInt64Ty()}, false));
      value = theBuilder->CreateCall(pull_fn, {value});
    }
    else if (styio_task_result_is_cstr(node->result_type)) {
      llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
      llvm::FunctionCallee pull_fn = theModule->getOrInsertFunction(
        "styio_task_cstr_pull",
        llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));
      value = theBuilder->CreateCall(pull_fn, {value});
    }
    else {
      llvm::FunctionCallee pull_fn = theModule->getOrInsertFunction(
        "styio_task_i64_pull",
        llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
      value = theBuilder->CreateCall(pull_fn, {value});
      if (node->result_type.option == StyioDataTypeOption::Bool) {
        value = theBuilder->CreateICmpNE(value, theBuilder->getInt64(0));
      }
    }
  }

  llvm::AllocaInst* slot = nullptr;
  auto it = mutable_variables.find(node->target_name);
  if (it != mutable_variables.end()) {
    slot = it->second;
  }
  else {
    slot = create_entry_alloca(node->toLLVMType(this), node->target_name);
    mutable_variables[node->target_name] = slot;
  }

  if (slot->getAllocatedType() == dynamic_cell_type()) {
    release_dynamic_slot_contents(slot);
    std::int64_t tag = STYIO_DYN_I64;
    llvm::Value* i64v = nullptr;
    llvm::Value* f64v = nullptr;
    llvm::Value* ptrv = nullptr;
    if (styio_task_result_is_f64(node->result_type)) {
      tag = STYIO_DYN_F64;
      f64v = value;
    }
    else if (styio_task_result_is_cstr(node->result_type)) {
      tag = STYIO_DYN_CSTR;
      ptrv = value;
    }
    else if (node->result_type.option == StyioDataTypeOption::Bool) {
      tag = STYIO_DYN_BOOL;
      i64v = value;
    }
    else {
      i64v = value;
    }
    store_dynamic_slot(slot, tag, i64v, f64v, ptrv);
    return value;
  }

  llvm::Type* want = slot->getAllocatedType();
  if (want->isIntegerTy(1) && !value->getType()->isIntegerTy(1)) {
    if (value->getType()->isIntegerTy()) {
      value = theBuilder->CreateICmpNE(value, llvm::ConstantInt::get(value->getType(), 0));
    }
  }
  else if (want->isIntegerTy(64) && value->getType()->isIntegerTy(1)) {
    value = theBuilder->CreateZExt(value, want);
  }
  else if (want->isIntegerTy() && value->getType()->isIntegerTy()
           && want != value->getType()) {
    value = theBuilder->CreateSExtOrTrunc(value, want);
  }
  else if (want->isDoubleTy() && value->getType()->isIntegerTy()) {
    value = theBuilder->CreateSIToFP(value, want);
  }
  else if (want->isIntegerTy() && value->getType()->isDoubleTy()) {
    value = theBuilder->CreateFPToSI(value, want);
  }

  theBuilder->CreateStore(value, slot);
  if (want->isPointerTy()) {
    forget_owned_cstr_temp(value);
  }
  return value;
}
