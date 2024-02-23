// [C++ STL]
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioToken/Token.hpp"
#include "Util.hpp"

void
display(
  string msg,
  StyioAST* ast
) {
  std::cout << msg << std::endl;
  std::cout << ast->toString() << std::endl;
}

void
display(
  StyioAST* ast,
  string msg = ""
) {
  std::cout << ast->toString() << std::endl;
  std::cout << msg << std::endl;
}

void
StyioToLLVM::check(CommentAST* ast) {
}

void
StyioToLLVM::check(NoneAST* ast) {
}

void
StyioToLLVM::check(EmptyAST* ast) {
}

void
StyioToLLVM::check(EmptyBlockAST* ast) {
}

void
StyioToLLVM::check(IdAST* ast) {
}

void
StyioToLLVM::check(DTypeAST* ast) {
}

void
StyioToLLVM::check(BoolAST* ast) {
}

void
StyioToLLVM::check(IntAST* ast) {
  if (ast->getType() == StyioDataType::undefined) {
    ast->setType(StyioDataType::i32);
  }
}

void
StyioToLLVM::check(FloatAST* ast) {
}

void
StyioToLLVM::check(CharAST* ast) {
}

void
StyioToLLVM::check(StringAST* ast) {
}

void
StyioToLLVM::check(TypeConvertAST*) {
}

void
StyioToLLVM::check(VarAST* ast) {
}

void
StyioToLLVM::check(ArgAST* ast) {
}

void
StyioToLLVM::check(OptArgAST* ast) {
}

void
StyioToLLVM::check(OptKwArgAST* ast) {
}

void
StyioToLLVM::check(FlexBindAST* ast) {
}

void
StyioToLLVM::check(FinalBindAST* ast) {
}

void
StyioToLLVM::check(InfiniteAST* ast) {
}

void
StyioToLLVM::check(StructAST* ast) {
}

void
StyioToLLVM::check(TupleAST* ast) {
}

void
StyioToLLVM::check(VarTupleAST* ast) {
}

void
StyioToLLVM::check(RangeAST* ast) {
}

void
StyioToLLVM::check(SetAST* ast) {
}

void
StyioToLLVM::check(ListAST* ast) {
}

void
StyioToLLVM::check(SizeOfAST* ast) {
}

void
StyioToLLVM::check(ListOpAST* ast) {
}

void
StyioToLLVM::check(BinCompAST* ast) {
}

void
StyioToLLVM::check(CondAST* ast) {
}

/*
  Int -> Int => Pass
  Int -> Float => Pass

*/
void
StyioToLLVM::check(BinOpAST* ast) {
  auto lhs = ast->getLhs();
  auto rhs = ast->getRhs();
  auto lhs_type = ast->getLhs()->hint();
  auto rhs_type = ast->getRhs()->hint();

  if (lhs_type != rhs_type) {
    if (lhs_type == StyioNodeHint::Int && rhs_type == StyioNodeHint::Float) {
      /* LHS: Int ~> Float */
      // ast -> setLhs(NumPromoAST::make(
      //   lhs,
      //   NumPromoTy::Int_To_Float)
      // );
    }
    else if (lhs_type == StyioNodeHint::Float && rhs_type == StyioNodeHint::Int) {
      /* RHS: Int ~> Float */
    }
  }
}

void
StyioToLLVM::check(FmtStrAST* ast) {
}

void
StyioToLLVM::check(ResourceAST* ast) {
}

void
StyioToLLVM::check(LocalPathAST* ast) {
}

void
StyioToLLVM::check(RemotePathAST* ast) {
}

void
StyioToLLVM::check(WebUrlAST* ast) {
}

void
StyioToLLVM::check(DBUrlAST* ast) {
}

void
StyioToLLVM::check(ExtPackAST* ast) {
}

void
StyioToLLVM::check(ReadFileAST* ast) {
}

void
StyioToLLVM::check(EOFAST* ast) {
}

void
StyioToLLVM::check(BreakAST* ast) {
}

void
StyioToLLVM::check(PassAST* ast) {
}

void
StyioToLLVM::check(ReturnAST* ast) {
}

void
StyioToLLVM::check(CallAST* ast) {
}

void
StyioToLLVM::check(PrintAST* ast) {
}

void
StyioToLLVM::check(ForwardAST* ast) {
}

void
StyioToLLVM::check(CheckEqAST* ast) {
}

void
StyioToLLVM::check(CheckIsInAST* ast) {
}

void
StyioToLLVM::check(FromToAST* ast) {
}

void
StyioToLLVM::check(CondFlowAST* ast) {
}

void
StyioToLLVM::check(AnonyFuncAST* ast) {
}

void
StyioToLLVM::check(FuncAST* ast) {
  
}

void
StyioToLLVM::check(IterAST* ast) {
}

void
StyioToLLVM::check(LoopAST* ast) {
}

void
StyioToLLVM::check(CasesAST* ast) {
}

void
StyioToLLVM::check(MatchCasesAST* ast) {
}

void
StyioToLLVM::check(BlockAST* ast) {
}

void
StyioToLLVM::check(MainBlockAST* ast) {
  auto& stmts = ast->getStmts();
  for (auto const& s : stmts) {
    s->check(this);
  }
}
