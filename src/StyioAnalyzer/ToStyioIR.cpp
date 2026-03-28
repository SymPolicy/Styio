/*
  Type Inference Implementation

  - Label Types
  - Find Recursive Type
*/

// [C++ STL]
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioIR/IOIR/IOIR.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "Util.hpp"

namespace {

SGType*
func_ret_to_sgtype(
  const std::variant<TypeAST*, TypeTupleAST*>& ret_type,
  StyioAnalyzer* an
) {
  if (ret_type.valueless_by_exception()) {
    return SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  }
  if (std::holds_alternative<TypeTupleAST*>(ret_type)) {
    return SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  }
  TypeAST* t = std::get<TypeAST*>(ret_type);
  if (!t || t->getDataType().option == StyioDataTypeOption::Undefined) {
    return SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  }
  return static_cast<SGType*>(t->toStyioIR(an));
}

SGFuncArg*
param_to_sgarg(ParamAST* p, StyioAnalyzer* an) {
  StyioDataTypeOption opty = p->var_type->getDataType().option;
  SGType* ty = (opty == StyioDataTypeOption::Undefined)
    ? SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64})
    : static_cast<SGType*>(p->var_type->toStyioIR(an));
  return SGFuncArg::Create(p->getName(), ty);
}

SGBlock*
lower_func_body(StyioAnalyzer* an, StyioAST* body) {
  if (!body) {
    return SGBlock::Create({});
  }
  if (auto* blk = dynamic_cast<BlockAST*>(body)) {
    return static_cast<SGBlock*>(blk->toStyioIR(an));
  }
  std::vector<StyioIR*> one;
  one.push_back(body->toStyioIR(an));
  return SGBlock::Create(std::move(one));
}

bool
stmt_has_return_tree(StyioAST* ast) {
  if (!ast) {
    return false;
  }
  if (ast->getNodeType() == StyioNodeType::Return) {
    return true;
  }
  if (ast->getNodeType() == StyioNodeType::MatchCases) {
    auto* m = static_cast<MatchCasesAST*>(ast);
    CasesAST* c = m->getCases();
    for (auto const& pr : c->case_list) {
      if (stmt_has_return_tree(pr.second)) {
        return true;
      }
    }
    return stmt_has_return_tree(c->case_default);
  }
  if (auto* b = dynamic_cast<BlockAST*>(ast)) {
    for (auto* s : b->stmts) {
      if (stmt_has_return_tree(s)) {
        return true;
      }
    }
  }
  return false;
}

void
scan_returns_for_str_int(StyioAST* ast, bool& has_str, bool& has_int) {
  if (!ast) {
    return;
  }
  if (ast->getNodeType() == StyioNodeType::Return) {
    StyioAST* e = static_cast<ReturnAST*>(ast)->getExpr();
    if (e->getNodeType() == StyioNodeType::String) {
      has_str = true;
    }
    else if (e->getNodeType() == StyioNodeType::Integer) {
      has_int = true;
    }
    else {
      has_int = true;
    }
    return;
  }
  if (ast->getNodeType() == StyioNodeType::MatchCases) {
    auto* m = static_cast<MatchCasesAST*>(ast);
    CasesAST* c = m->getCases();
    for (auto const& pr : c->case_list) {
      scan_returns_for_str_int(pr.second, has_str, has_int);
    }
    scan_returns_for_str_int(c->case_default, has_str, has_int);
    return;
  }
  if (auto* b = dynamic_cast<BlockAST*>(ast)) {
    for (auto* s : b->stmts) {
      scan_returns_for_str_int(s, has_str, has_int);
    }
  }
}

SGMatchReprKind
classify_cases(CasesAST* c) {
  bool any = false;
  for (auto const& pr : c->case_list) {
    if (stmt_has_return_tree(pr.second)) {
      any = true;
    }
  }
  if (stmt_has_return_tree(c->case_default)) {
    any = true;
  }
  if (!any) {
    return SGMatchReprKind::Stmt;
  }
  bool hs = false;
  bool hi = false;
  for (auto const& pr : c->case_list) {
    scan_returns_for_str_int(pr.second, hs, hi);
  }
  scan_returns_for_str_int(c->case_default, hs, hi);
  if (hs && hi) {
    return SGMatchReprKind::ExprMixed;
  }
  if (hs) {
    /* All arms yield strings (or only strings detected): phi must be i8* */
    return SGMatchReprKind::ExprMixed;
  }
  return SGMatchReprKind::ExprInt;
}

}  // namespace

static StyioOpType
comp_type_to_op(CompType ct) {
  switch (ct) {
    case CompType::EQ:
      return StyioOpType::Equal;
    case CompType::NE:
      return StyioOpType::Not_Equal;
    case CompType::GT:
      return StyioOpType::Greater_Than;
    case CompType::GE:
      return StyioOpType::Greater_Than_Equal;
    case CompType::LT:
      return StyioOpType::Less_Than;
    case CompType::LE:
      return StyioOpType::Less_Than_Equal;
    default:
      return StyioOpType::Equal;
  }
}

StyioIR*
StyioAnalyzer::toStyioIR(CommentAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(NoneAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(EmptyAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(NameAST* ast) {
  return SGResId::Create(ast->getAsStr());
}

StyioIR*
StyioAnalyzer::toStyioIR(TypeAST* ast) {
  return SGType::Create(ast->type);
}

StyioIR*
StyioAnalyzer::toStyioIR(TypeTupleAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(BoolAST* ast) {
  return SGConstBool::Create(ast->getValue());
}

StyioIR*
StyioAnalyzer::toStyioIR(IntAST* ast) {
  return SGConstInt::Create(ast->value, ast->num_of_bit);
}

StyioIR*
StyioAnalyzer::toStyioIR(FloatAST* ast) {
  return SGConstFloat::Create(ast->value);
}

StyioIR*
StyioAnalyzer::toStyioIR(CharAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(StringAST* ast) {
  const std::string& raw = ast->getValue();
  std::string inner = raw;
  if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
    inner = raw.substr(1, raw.size() - 2);
  }
  return SGConstString::Create(inner);
}

StyioIR*
StyioAnalyzer::toStyioIR(TypeConvertAST*) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(VarAST* ast) {
  if (ast->val_init) {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this)),
      ast->val_init->toStyioIR(this)
    );
  }
  else {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this))
    );
  }
}

StyioIR*
StyioAnalyzer::toStyioIR(ParamAST* ast) {
  if (ast->val_init) {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this)),
      ast->val_init->toStyioIR(this)
    );
  }
  else {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this))
    );
  }
}

StyioIR*
StyioAnalyzer::toStyioIR(OptArgAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(OptKwArgAST* ast) {
  return SGConstInt::Create(0);
}

/*
  The declared type is always the *top* priority
  because the programmer wrote in that way!
*/
StyioIR*
StyioAnalyzer::toStyioIR(FlexBindAST* ast) {
  return SGFlexBind::Create(
    static_cast<SGVar*>(ast->getVar()->toStyioIR(this)), 
    ast->getValue()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(FinalBindAST* ast) {
  return SGFinalBind::Create(
    static_cast<SGVar*>(ast->getVar()->toStyioIR(this)), 
    ast->getValue()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(InfiniteAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(StructAST* ast) {
  std::vector<SGVar*> elems;

  for (auto arg : ast->args) {
    elems.push_back(static_cast<SGVar*>(arg->toStyioIR(this)));
  }

  return SGStruct::Create(SGResId::Create(ast->name->getAsStr()), elems);
}

StyioIR*
StyioAnalyzer::toStyioIR(TupleAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(VarTupleAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ExtractorAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(RangeAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(SetAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ListAST* ast) {
  std::vector<StyioIR*> el;
  for (auto* e : ast->getElements()) {
    el.push_back(e->toStyioIR(this));
  }
  return SGListLiteral::Create(std::move(el));
}

StyioIR*
StyioAnalyzer::toStyioIR(SizeOfAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ListOpAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(BinCompAST* ast) {
  StyioOpType op = comp_type_to_op(ast->getSign());
  return SGBinOp::Create(
    ast->getLHS()->toStyioIR(this),
    ast->getRHS()->toStyioIR(this),
    op,
    SGType::Create(StyioDataType{StyioDataTypeOption::Bool, "bool", 1}));
}

StyioIR*
StyioAnalyzer::toStyioIR(CondAST* ast) {
  switch (ast->getSign()) {
    case LogicType::AND:
      return SGCond::Create(
        ast->getLHS()->toStyioIR(this),
        ast->getRHS()->toStyioIR(this),
        StyioOpType::Logic_AND);
    case LogicType::OR:
      return SGCond::Create(
        ast->getLHS()->toStyioIR(this),
        ast->getRHS()->toStyioIR(this),
        StyioOpType::Logic_OR);
    case LogicType::RAW:
      return ast->getValue()->toStyioIR(this);
    default:
      return ast->getValue() ? ast->getValue()->toStyioIR(this) : SGConstInt::Create(0);
  }
}

StyioIR*
StyioAnalyzer::toStyioIR(UndefinedLitAST* ast) {
  (void)ast;
  return SGUndef::Create();
}

StyioIR*
StyioAnalyzer::toStyioIR(WaveMergeAST* ast) {
  return SGWaveMerge::Create(
    ast->getCond()->toStyioIR(this),
    ast->getTrueVal()->toStyioIR(this),
    ast->getFalseVal()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(WaveDispatchAST* ast) {
  return SGWaveDispatch::Create(
    ast->getCond()->toStyioIR(this),
    ast->getTrueArm()->toStyioIR(this),
    ast->getFalseArm()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(FallbackAST* ast) {
  return SGFallback::Create(
    ast->getPrimary()->toStyioIR(this),
    ast->getAlternate()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(GuardSelectorAST* ast) {
  return SGGuardSelect::Create(
    ast->getBase()->toStyioIR(this),
    ast->getCond()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(EqProbeAST* ast) {
  return SGEqProbe::Create(
    ast->getBase()->toStyioIR(this),
    ast->getProbeValue()->toStyioIR(this));
}

/*
  Int -> Int => Pass
  Int -> Float => Pass
*/
StyioIR*
StyioAnalyzer::toStyioIR(BinOpAST* ast) {
  return SGBinOp::Create(
    ast->LHS->toStyioIR(this),
    ast->RHS->toStyioIR(this),
    ast->operand,
    static_cast<SGType*>(ast->data_type->toStyioIR(this)));
}

StyioIR*
StyioAnalyzer::toStyioIR(FmtStrAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ResourceAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ResPathAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(RemotePathAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(WebUrlAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(DBUrlAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ExtPackAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ReadFileAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(EOFAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(BreakAST* ast) {
  return SGBreak::Create(ast->getDepth());
}

StyioIR*
StyioAnalyzer::toStyioIR(ContinueAST* ast) {
  return SGContinue::Create(ast->getDepth());
}

StyioIR*
StyioAnalyzer::toStyioIR(PassAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ReturnAST* ast) {
  return SGReturn::Create(ast->getExpr()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(FuncCallAST* ast) {
  std::vector<StyioIR*> args;
  for (auto* a : ast->getArgList()) {
    args.push_back(a->toStyioIR(this));
  }
  return SGCall::Create(
    SGResId::Create(ast->getNameAsStr()),
    std::move(args));
}

StyioIR*
StyioAnalyzer::toStyioIR(AttrAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(PrintAST* ast) {
  std::vector<StyioIR*> parts;
  for (auto* e : ast->exprs) {
    parts.push_back(e->toStyioIR(this));
  }
  return SIOPrint::Create(parts);
}

StyioIR*
StyioAnalyzer::toStyioIR(ForwardAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(BackwardAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(CODPAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(CheckEqualAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(CheckIsinAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(HashTagNameAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(CondFlowAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(AnonyFuncAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(FunctionAST* ast) {
  std::vector<SGFuncArg*> fargs;
  for (auto* p : ast->params) {
    fargs.push_back(param_to_sgarg(p, this));
  }
  SGType* rt = func_ret_to_sgtype(ast->ret_type, this);
  if (auto* blk = dynamic_cast<BlockAST*>(ast->func_body)) {
    if (blk->stmts.size() == 1 && blk->stmts[0]->getNodeType() == StyioNodeType::MatchCases) {
      auto* mc = static_cast<MatchCasesAST*>(blk->stmts[0]);
      CasesAST* c = mc->getCases();
      bool hs = false;
      bool hi = false;
      for (auto const& pr : c->case_list) {
        scan_returns_for_str_int(pr.second, hs, hi);
      }
      scan_returns_for_str_int(c->case_default, hs, hi);
      if (hs) {
        rt = SGType::Create(StyioDataType{StyioDataTypeOption::String, "string", 0});
      }
    }
  }
  SGBlock* body = lower_func_body(this, ast->func_body);
  return SGFunc::Create(
    rt,
    SGResId::Create(ast->getNameAsStr()),
    std::move(fargs),
    body);
}

StyioIR*
StyioAnalyzer::toStyioIR(SimpleFuncAST* ast) {
  std::vector<SGFuncArg*> fargs;
  for (auto* p : ast->params) {
    fargs.push_back(param_to_sgarg(p, this));
  }
  SGType* rt = func_ret_to_sgtype(ast->ret_type, this);
  if (auto* blk = dynamic_cast<BlockAST*>(ast->ret_expr)) {
    if (blk->stmts.size() == 1 && blk->stmts[0]->getNodeType() == StyioNodeType::MatchCases) {
      auto* mc = static_cast<MatchCasesAST*>(blk->stmts[0]);
      CasesAST* c = mc->getCases();
      bool hs = false;
      bool hi = false;
      for (auto const& pr : c->case_list) {
        scan_returns_for_str_int(pr.second, hs, hi);
      }
      scan_returns_for_str_int(c->case_default, hs, hi);
      if (hs) {
        /* Any <| "..." arm: LLVM return must be i8* (and may mix with snprintf ints). */
        rt = SGType::Create(StyioDataType{StyioDataTypeOption::String, "string", 0});
      }
    }
  }
  SGBlock* body = lower_func_body(this, ast->ret_expr);
  return SGFunc::Create(
    rt,
    SGResId::Create(ast->func_name->getAsStr()),
    std::move(fargs),
    body);
}

StyioIR*
StyioAnalyzer::toStyioIR(IteratorAST* ast) {
  std::string vname = "it";
  if (!ast->params.empty()) {
    vname = ast->params[0]->getName();
  }
  SGBlock* body = SGBlock::Create({});
  if (!ast->following.empty()) {
    body = lower_func_body(this, ast->following[0]);
  }
  return SGForEach::Create(ast->collection->toStyioIR(this), std::move(vname), body);
}

StyioIR*
StyioAnalyzer::toStyioIR(IterSeqAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(InfiniteLoopAST* ast) {
  SGBlock* b = lower_func_body(this, ast->getBody());
  if (ast->getWhileCond()) {
    return SGLoop::CreateWhile(ast->getWhileCond()->toStyioIR(this), b);
  }
  return SGLoop::CreateInfinite(b);
}

StyioIR*
StyioAnalyzer::toStyioIR(CasesAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(MatchCasesAST* ast) {
  CasesAST* c = ast->getCases();
  SGMatchReprKind rk = classify_cases(c);
  StyioIR* scr = ast->getScrutinee()->toStyioIR(this);
  std::vector<std::pair<std::int64_t, SGBlock*>> arms;
  for (auto const& pr : c->case_list) {
    auto* li = dynamic_cast<IntAST*>(pr.first);
    if (!li) {
      throw StyioNotImplemented("match arms need integer literal patterns in this milestone");
    }
    arms.push_back({std::stoll(li->value), lower_func_body(this, pr.second)});
  }
  SGBlock* def = nullptr;
  if (c->case_default) {
    def = lower_func_body(this, c->case_default);
  }
  return SGMatch::Create(scr, std::move(arms), def, rk);
}

StyioIR*
StyioAnalyzer::toStyioIR(BlockAST* ast) {
  std::vector<StyioIR*> ir_stmts;
  for (auto* s : ast->stmts) {
    ir_stmts.push_back(s->toStyioIR(this));
  }
  return SGBlock::Create(ir_stmts);
}

StyioIR*
StyioAnalyzer::toStyioIR(MainBlockAST* ast) {
  std::vector<StyioIR*> ir_stmts;

  for (auto stmt : ast->getStmts()) {
    ir_stmts.push_back(stmt->toStyioIR(this));
  }

  return SGMainEntry::Create(ir_stmts);
}
