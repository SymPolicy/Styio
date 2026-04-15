// M6: pulse ledger, frame snapshot, intrinsics — codegen (StyioToLLVM members).

#include "CodeGenVisitor.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include <limits>

namespace {

int64_t
k_undef_i64() {
  return std::numeric_limits<int64_t>::min();
}

int
abs_value_off(const SGStateSlotDesc& s) {
  if (s.kind == SGStateSlotKind::WinAvg || s.kind == SGStateSlotKind::WinMax) {
    return s.offset + s.win_n * 8 + 24;
  }
  return s.offset;
}

int
abs_hist_ring(const SGStateSlotDesc& s) {
  if (s.kind == SGStateSlotKind::Track) {
    return s.offset + 8;
  }
  return s.offset + s.win_n * 8 + 32;
}

int
abs_hist_hpos(const SGStateSlotDesc& s) {
  if (s.kind == SGStateSlotKind::Track) {
    return s.offset + 8 + 8 * s.win_n;
  }
  return s.offset + s.win_n * 8 + 32 + s.win_n * 8;
}

int
abs_ring(const SGStateSlotDesc& s) {
  return s.offset;
}

int
abs_sum(const SGStateSlotDesc& s) {
  return s.offset + s.win_n * 8;
}

int
abs_cur(const SGStateSlotDesc& s) {
  return s.offset + s.win_n * 8 + 8;
}

int
abs_cnt(const SGStateSlotDesc& s) {
  return s.offset + s.win_n * 8 + 16;
}

}  // namespace

llvm::Value*
StyioToLLVM::styio_load_i64_at_byte_ptr(llvm::Value* base_i8, int byte_off) {
  llvm::Type* i8t = theBuilder->getInt8Ty();
  llvm::Type* i64t = theBuilder->getInt64Ty();
  llvm::Value* p = theBuilder->CreateInBoundsGEP(
    i8t,
    base_i8,
    llvm::ConstantInt::get(theBuilder->getInt64Ty(), byte_off));
  llvm::Value* pi64 = theBuilder->CreateBitCast(p, llvm::PointerType::get(i64t, 0));
  return theBuilder->CreateLoad(i64t, pi64);
}

void
StyioToLLVM::styio_store_i64_at_byte_ptr(
  llvm::Value* base_i8,
  int byte_off,
  llvm::Value* v
) {
  llvm::Type* i8t = theBuilder->getInt8Ty();
  llvm::Type* i64t = theBuilder->getInt64Ty();
  llvm::Value* p = theBuilder->CreateInBoundsGEP(
    i8t,
    base_i8,
    llvm::ConstantInt::get(theBuilder->getInt64Ty(), byte_off));
  llvm::Value* pi64 = theBuilder->CreateBitCast(p, llvm::PointerType::get(i64t, 0));
  theBuilder->CreateStore(v, pi64);
}

void
StyioToLLVM::pulse_copy_ledger_to_snap(
  llvm::Value* ledger_i8,
  llvm::Value* snap_i8,
  int nbytes
) {
  for (int i = 0; i < nbytes; i += 8) {
    llvm::Value* v = styio_load_i64_at_byte_ptr(ledger_i8, i);
    styio_store_i64_at_byte_ptr(snap_i8, i, v);
  }
}

llvm::Value*
StyioToLLVM::coerce_pulse_input_i64(llvm::Value* v) {
  if (v->getType()->isPointerTy()) {
    return cstr_to_i64_checked(v);
  }
  return v;
}

void
StyioToLLVM::emit_pulse_commit_all(llvm::Value* ledger_i8, const SGPulsePlan* plan) {
  if (!plan) {
    return;
  }
  llvm::Type* i64t = theBuilder->getInt64Ty();
  for (auto const& pr : plan->commits) {
    int sid = pr.first;
    const std::string& en = pr.second;
    const SGStateSlotDesc& s = plan->slots[static_cast<size_t>(sid)];
    auto itv = mutable_variables.find(en);
    if (itv == mutable_variables.end()) {
      continue;
    }
    llvm::Value* newv = theBuilder->CreateLoad(i64t, itv->second);

    switch (s.kind) {
      case SGStateSlotKind::Acc: {
        styio_store_i64_at_byte_ptr(ledger_i8, s.offset, newv);
      } break;

      case SGStateSlotKind::Track: {
        llvm::Value* oldv = styio_load_i64_at_byte_ptr(ledger_i8, s.offset);
        int W = s.win_n;
        llvm::Value* hp = styio_load_i64_at_byte_ptr(ledger_i8, abs_hist_hpos(s));
        llvm::Value* wi = theBuilder->CreateURem(
          hp,
          llvm::ConstantInt::get(i64t, static_cast<uint64_t>(W)));
        llvm::Value* bo = theBuilder->CreateAdd(
          llvm::ConstantInt::get(i64t, abs_hist_ring(s)),
          theBuilder->CreateMul(wi, llvm::ConstantInt::get(i64t, 8)));
        llvm::Value* g = theBuilder->CreateInBoundsGEP(
          theBuilder->getInt8Ty(),
          ledger_i8,
          bo);
        llvm::Value* pc = theBuilder->CreateBitCast(g, llvm::PointerType::get(i64t, 0));
        theBuilder->CreateStore(oldv, pc);
        llvm::Value* hp1 = theBuilder->CreateAdd(hp, llvm::ConstantInt::get(i64t, 1));
        styio_store_i64_at_byte_ptr(ledger_i8, abs_hist_hpos(s), hp1);
        styio_store_i64_at_byte_ptr(ledger_i8, s.offset, newv);
      } break;

      case SGStateSlotKind::WinAvg:
      case SGStateSlotKind::WinMax: {
        llvm::Value* old_last = styio_load_i64_at_byte_ptr(ledger_i8, abs_value_off(s));
        int W = s.win_n;
        llvm::Value* hp = styio_load_i64_at_byte_ptr(ledger_i8, abs_hist_hpos(s));
        llvm::Value* wi = theBuilder->CreateURem(
          hp,
          llvm::ConstantInt::get(i64t, static_cast<uint64_t>(W)));
        llvm::Value* bo = theBuilder->CreateAdd(
          llvm::ConstantInt::get(i64t, abs_hist_ring(s)),
          theBuilder->CreateMul(wi, llvm::ConstantInt::get(i64t, 8)));
        llvm::Value* g = theBuilder->CreateInBoundsGEP(
          theBuilder->getInt8Ty(),
          ledger_i8,
          bo);
        llvm::Value* pc = theBuilder->CreateBitCast(g, llvm::PointerType::get(i64t, 0));
        theBuilder->CreateStore(old_last, pc);
        llvm::Value* hp1 = theBuilder->CreateAdd(hp, llvm::ConstantInt::get(i64t, 1));
        styio_store_i64_at_byte_ptr(ledger_i8, abs_hist_hpos(s), hp1);
        styio_store_i64_at_byte_ptr(ledger_i8, abs_value_off(s), newv);
      } break;

      default:
        break;
    }
  }
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGStateSnapLoad* node) {
  if (!pulse_active_plan_ || !pulse_snap_base_) {
    return theBuilder->getInt64(0);
  }
  const SGStateSlotDesc& s = pulse_active_plan_->slots[static_cast<size_t>(node->slot_id)];
  return styio_load_i64_at_byte_ptr(pulse_snap_base_, abs_value_off(s));
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGStateHistLoad* node) {
  llvm::Value* base_i8 = nullptr;
  const SGPulsePlan* plan = nullptr;
  if (node->pulse_region_id >= 0) {
    auto it = pulse_region_ledgers_.find(node->pulse_region_id);
    if (it == pulse_region_ledgers_.end()) {
      return theBuilder->getInt64(0);
    }
    base_i8 = it->second.first;
    plan = it->second.second;
  }
  else {
    if (!pulse_active_plan_ || !pulse_snap_base_) {
      return theBuilder->getInt64(0);
    }
    base_i8 = pulse_snap_base_;
    plan = pulse_active_plan_;
  }
  const SGStateSlotDesc& s = plan->slots[static_cast<size_t>(node->slot_id)];
  llvm::Type* i64t = theBuilder->getInt64Ty();
  llvm::Value* hp = styio_load_i64_at_byte_ptr(base_i8, abs_hist_hpos(s));
  int W = s.win_n;
  int d = node->depth;
  /* Snap (in-pulse): hp is from frame start. Post-loop ledger: hp already bumped after
     last commit, so use hp - d instead of hp - (1+d). */
  int64_t sub = (node->pulse_region_id >= 0)
    ? static_cast<int64_t>(d)
    : static_cast<int64_t>(1 + d);
  llvm::Value* idx = theBuilder->CreateSub(hp, llvm::ConstantInt::get(i64t, sub));
  llvm::Value* wi = theBuilder->CreateURem(idx, llvm::ConstantInt::get(i64t, static_cast<uint64_t>(W)));
  llvm::Value* bo = theBuilder->CreateAdd(
    llvm::ConstantInt::get(i64t, abs_hist_ring(s)),
    theBuilder->CreateMul(wi, llvm::ConstantInt::get(i64t, 8)));
  llvm::Value* g = theBuilder->CreateInBoundsGEP(
    theBuilder->getInt8Ty(),
    base_i8,
    bo);
  llvm::Value* pc = theBuilder->CreateBitCast(g, llvm::PointerType::get(i64t, 0));
  return theBuilder->CreateLoad(theBuilder->getInt64Ty(), pc);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGSeriesAvgStep* node) {
  if (!pulse_active_plan_ || !pulse_ledger_base_) {
    return theBuilder->getInt64(0);
  }
  const SGStateSlotDesc& s = pulse_active_plan_->slots[static_cast<size_t>(node->slot_id)];
  llvm::Type* i64t = theBuilder->getInt64Ty();
  llvm::Value* u = llvm::ConstantInt::get(i64t, k_undef_i64());

  llvm::Value* xv = node->x->toLLVMIR(this);
  xv = coerce_pulse_input_i64(xv);
  llvm::Value* isu = theBuilder->CreateICmpEQ(xv, u);

  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock* ok_bb = llvm::BasicBlock::Create(*theContext, "sgavg_ok", F);
  llvm::BasicBlock* bad_bb = llvm::BasicBlock::Create(*theContext, "sgavg_bad", F);
  llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*theContext, "sgavg_m", F);
  theBuilder->CreateCondBr(isu, bad_bb, ok_bb);

  theBuilder->SetInsertPoint(bad_bb);
  theBuilder->CreateBr(merge_bb);

  theBuilder->SetInsertPoint(ok_bb);
  int n = s.win_n;
  llvm::Value* cur = styio_load_i64_at_byte_ptr(pulse_ledger_base_, abs_cur(s));
  llvm::Value* cnt = styio_load_i64_at_byte_ptr(pulse_ledger_base_, abs_cnt(s));
  llvm::Value* sum = styio_load_i64_at_byte_ptr(pulse_ledger_base_, abs_sum(s));
  llvm::Value* cur_w = theBuilder->CreateURem(
    cur,
    llvm::ConstantInt::get(i64t, static_cast<uint64_t>(n)));
  llvm::Value* bo_old = theBuilder->CreateAdd(
    llvm::ConstantInt::get(i64t, abs_ring(s)),
    theBuilder->CreateMul(cur_w, llvm::ConstantInt::get(i64t, 8)));
  llvm::Value* g_old = theBuilder->CreateInBoundsGEP(
    theBuilder->getInt8Ty(),
    pulse_ledger_base_,
    bo_old);
  llvm::Value* p_old = theBuilder->CreateBitCast(g_old, llvm::PointerType::get(i64t, 0));
  llvm::Value* old = theBuilder->CreateLoad(i64t, p_old);
  llvm::Value* sum2 = theBuilder->CreateSub(theBuilder->CreateAdd(sum, xv), old);
  theBuilder->CreateStore(xv, p_old);
  llvm::Value* cur2 = theBuilder->CreateAdd(cur, llvm::ConstantInt::get(i64t, 1));
  llvm::Value* cntp1 = theBuilder->CreateAdd(cnt, llvm::ConstantInt::get(i64t, 1));
  llvm::Value* cnt2 = theBuilder->CreateSelect(
    theBuilder->CreateICmpSLT(cntp1, llvm::ConstantInt::get(i64t, n)),
    cntp1,
    llvm::ConstantInt::get(i64t, n));
  styio_store_i64_at_byte_ptr(pulse_ledger_base_, abs_sum(s), sum2);
  styio_store_i64_at_byte_ptr(pulse_ledger_base_, abs_cur(s), cur2);
  styio_store_i64_at_byte_ptr(pulse_ledger_base_, abs_cnt(s), cnt2);
  llvm::Value* warm = theBuilder->CreateICmpSGE(cnt2, llvm::ConstantInt::get(i64t, n));
  llvm::Value* avgv = theBuilder->CreateSDiv(sum2, llvm::ConstantInt::get(i64t, n));
  llvm::Value* out_ok = theBuilder->CreateSelect(warm, avgv, u);
  theBuilder->CreateBr(merge_bb);

  theBuilder->SetInsertPoint(merge_bb);
  llvm::PHINode* phi = theBuilder->CreatePHI(i64t, 2, "sgavg_phi");
  phi->addIncoming(u, bad_bb);
  phi->addIncoming(out_ok, ok_bb);
  return phi;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGSeriesMaxStep* node) {
  if (!pulse_active_plan_ || !pulse_ledger_base_) {
    return theBuilder->getInt64(0);
  }
  const SGStateSlotDesc& s = pulse_active_plan_->slots[static_cast<size_t>(node->slot_id)];
  llvm::Type* i64t = theBuilder->getInt64Ty();
  llvm::Value* u = llvm::ConstantInt::get(i64t, k_undef_i64());
  llvm::Value* xv = coerce_pulse_input_i64(node->x->toLLVMIR(this));
  llvm::Value* isu = theBuilder->CreateICmpEQ(xv, u);

  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock* bad_bb = llvm::BasicBlock::Create(*theContext, "sgmax_bad", F);
  llvm::BasicBlock* ok_bb = llvm::BasicBlock::Create(*theContext, "sgmax_ok", F);
  llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*theContext, "sgmax_m", F);
  theBuilder->CreateCondBr(isu, bad_bb, ok_bb);

  theBuilder->SetInsertPoint(bad_bb);
  theBuilder->CreateBr(merge_bb);

  theBuilder->SetInsertPoint(ok_bb);
  int n = s.win_n;
  llvm::Value* wp = styio_load_i64_at_byte_ptr(pulse_ledger_base_, abs_cur(s));
  llvm::Value* cnt = styio_load_i64_at_byte_ptr(pulse_ledger_base_, abs_cnt(s));
  llvm::Value* wi = theBuilder->CreateURem(
    wp,
    llvm::ConstantInt::get(i64t, static_cast<uint64_t>(n)));
  llvm::Value* bo = theBuilder->CreateAdd(
    llvm::ConstantInt::get(i64t, abs_ring(s)),
    theBuilder->CreateMul(wi, llvm::ConstantInt::get(i64t, 8)));
  llvm::Value* g0 = theBuilder->CreateInBoundsGEP(
    theBuilder->getInt8Ty(),
    pulse_ledger_base_,
    bo);
  llvm::Value* pr = theBuilder->CreateBitCast(g0, llvm::PointerType::get(i64t, 0));
  theBuilder->CreateStore(xv, pr);
  llvm::Value* wp2 = theBuilder->CreateAdd(wp, llvm::ConstantInt::get(i64t, 1));
  llvm::Value* cntp1 = theBuilder->CreateAdd(cnt, llvm::ConstantInt::get(i64t, 1));
  llvm::Value* cnt2 = theBuilder->CreateSelect(
    theBuilder->CreateICmpSLT(cntp1, llvm::ConstantInt::get(i64t, n)),
    cntp1,
    llvm::ConstantInt::get(i64t, n));
  styio_store_i64_at_byte_ptr(pulse_ledger_base_, abs_cur(s), wp2);
  styio_store_i64_at_byte_ptr(pulse_ledger_base_, abs_cnt(s), cnt2);

  llvm::BasicBlock* loop_hdr = llvm::BasicBlock::Create(*theContext, "sgmax_lh", F);
  llvm::BasicBlock* loop_bd = llvm::BasicBlock::Create(*theContext, "sgmax_lb", F);
  llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(*theContext, "sgmax_le", F);
  theBuilder->CreateBr(loop_hdr);
  theBuilder->SetInsertPoint(loop_hdr);
  llvm::PHINode* phi_k = theBuilder->CreatePHI(i64t, 2, "sgmax_k");
  llvm::PHINode* phi_best = theBuilder->CreatePHI(i64t, 2, "sgmax_best");
  phi_k->addIncoming(llvm::ConstantInt::get(i64t, 1), ok_bb);
  phi_best->addIncoming(xv, ok_bb);
  llvm::Value* go = theBuilder->CreateICmpSLT(phi_k, cnt2);
  theBuilder->CreateCondBr(go, loop_bd, loop_end);

  theBuilder->SetInsertPoint(loop_bd);
  llvm::Value* back = theBuilder->CreateSub(wp2, phi_k);
  llvm::Value* idx = theBuilder->CreateURem(
    back,
    llvm::ConstantInt::get(i64t, static_cast<uint64_t>(n)));
  llvm::Value* bo2 = theBuilder->CreateAdd(
    llvm::ConstantInt::get(i64t, abs_ring(s)),
    theBuilder->CreateMul(idx, llvm::ConstantInt::get(i64t, 8)));
  llvm::Value* g2 = theBuilder->CreateInBoundsGEP(
    theBuilder->getInt8Ty(),
    pulse_ledger_base_,
    bo2);
  llvm::Value* pr2 = theBuilder->CreateBitCast(g2, llvm::PointerType::get(i64t, 0));
  llvm::Value* cell = theBuilder->CreateLoad(i64t, pr2);
  llvm::Value* gt = theBuilder->CreateICmpSGT(cell, phi_best);
  llvm::Value* best2 = theBuilder->CreateSelect(gt, cell, phi_best);
  llvm::Value* k2 = theBuilder->CreateAdd(phi_k, llvm::ConstantInt::get(i64t, 1));
  theBuilder->CreateBr(loop_hdr);
  phi_k->addIncoming(k2, loop_bd);
  phi_best->addIncoming(best2, loop_bd);

  theBuilder->SetInsertPoint(loop_end);
  llvm::Value* cold = theBuilder->CreateICmpSLT(cnt2, llvm::ConstantInt::get(i64t, n));
  llvm::Value* out_ok = theBuilder->CreateSelect(cold, u, phi_best);
  theBuilder->CreateBr(merge_bb);

  theBuilder->SetInsertPoint(merge_bb);
  llvm::PHINode* pm = theBuilder->CreatePHI(i64t, 2, "sgmax_phi");
  pm->addIncoming(u, bad_bb);
  pm->addIncoming(out_ok, loop_end);
  return pm;
}
