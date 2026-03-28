/*
  Type Inference Implementation

  - Label Types
  - Find Recursive Type
*/

// [C++ STL]
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioIR/IOIR/IOIR.hpp"
#include "../StyioToken/Token.hpp"
#include "Util.hpp"

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
  return SGConstInt::Create(0);
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
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(PassAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ReturnAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(FuncCallAST* ast) {
  return SGConstInt::Create(0);
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
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(SimpleFuncAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(IteratorAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(IterSeqAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(InfiniteLoopAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(CasesAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(MatchCasesAST* ast) {
  return SGConstInt::Create(0);
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
