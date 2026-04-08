// [C++ STL]
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
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

llvm::Value*
StyioToLLVM::toLLVMIR(SIOPath* node) {
  auto output = theBuilder->getInt32(0);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOPrint* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee printf_fn = theModule->getOrInsertFunction(
    "printf",
    llvm::FunctionType::get(theBuilder->getInt32Ty(), char_ptr, true));

  llvm::FunctionCallee puts_fn = theModule->getOrInsertFunction(
    "puts",
    llvm::FunctionType::get(theBuilder->getInt32Ty(), char_ptr, false));

  for (StyioIR* part : node->expr) {
    llvm::Value* v = part->toLLVMIR(this);

    if (v->getType()->isIntegerTy(1)) {
      llvm::Value* tstr = theBuilder->CreateGlobalStringPtr("true", "styio_true_nl");
      llvm::Value* fstr = theBuilder->CreateGlobalStringPtr("false", "styio_false_nl");
      llvm::Value* pick = theBuilder->CreateSelect(v, tstr, fstr);
      theBuilder->CreateCall(puts_fn, {pick});
    }
    else if (v->getType()->isIntegerTy(32)) {
      llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%d\n", "styio_fmt_i32");
      theBuilder->CreateCall(printf_fn, {fmt, v});
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
      theBuilder->CreateCall(puts_fn, {ats});
      theBuilder->CreateBr(b_done);
      theBuilder->SetInsertPoint(b_num);
      llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%lld\n", "styio_fmt_i64");
      theBuilder->CreateCall(printf_fn, {fmt, v});
      theBuilder->CreateBr(b_done);
      theBuilder->SetInsertPoint(b_done);
    }
    else if (v->getType()->isDoubleTy()) {
      llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%.6f\n", "styio_fmt_f64");
      theBuilder->CreateCall(printf_fn, {fmt, v});
    }
    else if (v->getType()->isPointerTy()) {
      llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%s\n", "styio_fmt_str");
      theBuilder->CreateCall(printf_fn, {fmt, v});
      free_owned_cstr_temp_if_tracked(v);
    }
    else {
      llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%lld\n", "styio_fmt_fallback");
      llvm::Value* as_i64 = theBuilder->CreatePtrToInt(v, theBuilder->getInt64Ty());
      theBuilder->CreateCall(printf_fn, {fmt, as_i64});
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
    llvm::FunctionCallee printf_fn = theModule->getOrInsertFunction(
      "printf",
      llvm::FunctionType::get(theBuilder->getInt32Ty(), char_ptr, true));

    llvm::FunctionCallee puts_fn = theModule->getOrInsertFunction(
      "puts",
      llvm::FunctionType::get(theBuilder->getInt32Ty(), char_ptr, false));

    for (StyioIR* part : node->exprs) {
      llvm::Value* v = part->toLLVMIR(this);

      if (v->getType()->isIntegerTy(1)) {
        llvm::Value* tstr = theBuilder->CreateGlobalStringPtr("true", "styio_true_nl");
        llvm::Value* fstr = theBuilder->CreateGlobalStringPtr("false", "styio_false_nl");
        llvm::Value* pick = theBuilder->CreateSelect(v, tstr, fstr);
        theBuilder->CreateCall(puts_fn, {pick});
      }
      else if (v->getType()->isIntegerTy(32)) {
        llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%d\n", "styio_fmt_i32");
        theBuilder->CreateCall(printf_fn, {fmt, v});
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
        theBuilder->CreateCall(puts_fn, {ats});
        theBuilder->CreateBr(b_done);
        theBuilder->SetInsertPoint(b_num);
        llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%lld\n", "styio_fmt_i64");
        theBuilder->CreateCall(printf_fn, {fmt, v});
        theBuilder->CreateBr(b_done);
        theBuilder->SetInsertPoint(b_done);
      }
      else if (v->getType()->isDoubleTy()) {
        llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%.6f\n", "styio_fmt_f64");
        theBuilder->CreateCall(printf_fn, {fmt, v});
      }
      else if (v->getType()->isPointerTy()) {
        llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%s\n", "styio_fmt_str");
        theBuilder->CreateCall(printf_fn, {fmt, v});
        free_owned_cstr_temp_if_tracked(v);
      }
      else {
        llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%lld\n", "styio_fmt_fallback");
        llvm::Value* as_i64 = theBuilder->CreatePtrToInt(v, theBuilder->getInt64Ty());
        theBuilder->CreateCall(printf_fn, {fmt, as_i64});
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

  /* Pulse plan setup (same pattern as SGFileLineIter). */
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

  llvm::FunctionCallee to_i64_fn = theModule->getOrInsertFunction(
    "styio_cstr_to_i64",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));

  llvm::Value* line = theBuilder->CreateCall(read_fn, {});
  /* Convert to i64 (matches InstantPullAST's data type convention). */
  return theBuilder->CreateCall(to_i64_fn, {line});
}
