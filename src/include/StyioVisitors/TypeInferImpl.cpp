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
StyioASTAnalyzer::typeInfer(CommentAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(NoneAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(EmptyAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(IdAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(DTypeAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(BoolAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(IntAST* ast) {
  if (ast->getType() == StyioDataType::undefined) {
    ast->setType(StyioDataType::i32);
  }
}

void
StyioASTAnalyzer::typeInfer(FloatAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(CharAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(StringAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(TypeConvertAST*) {
}

void
StyioASTAnalyzer::typeInfer(VarAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(ArgAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(OptArgAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(OptKwArgAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(FlexBindAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(FinalBindAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(InfiniteAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(StructAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(TupleAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(VarTupleAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(RangeAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(SetAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(ListAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(SizeOfAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(ListOpAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(BinCompAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(CondAST* ast) {
}

/*
  Int -> Int => Pass
  Int -> Float => Pass

*/
void
StyioASTAnalyzer::typeInfer(BinOpAST* ast) {
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
StyioASTAnalyzer::typeInfer(FmtStrAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(ResourceAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(LocalPathAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(RemotePathAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(WebUrlAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(DBUrlAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(ExtPackAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(ReadFileAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(EOFAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(BreakAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(PassAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(ReturnAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(CallAST* ast) {
  if (not func_defs.contains(ast->getName())) {
    std::cout << "func " << ast->getName() << " not exist" << std::endl;
    return;
  }

  vector<StyioDataType> arg_types;

  for (auto arg : ast->getArgs()) {
    StyioDataType data_type;

    switch (arg->hint()) {
      case StyioNodeHint::Int: {
        
        arg_types.push_back(static_cast<IntAST*>(arg)->getType());
      } break;

      default:
        break;
    }
  }

  auto func_args = func_defs[ast->getName()]->getAllArgs();

  if (arg_types.size() != func_args.size()) {
    std::cout << "arg list not match" << std::endl;
    return;
  }

  for (size_t i = 0; i < func_args.size(); i++) {
    func_args[i]->setDType(arg_types[i]);
  }
}

void
StyioASTAnalyzer::typeInfer(PrintAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(ForwardAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(CheckEqAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(CheckIsinAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(FromToAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(CondFlowAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(AnonyFuncAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(FuncAST* ast) {
  func_defs[ast->getFuncName()] = ast;
}

void
StyioASTAnalyzer::typeInfer(IterAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(LoopAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(CasesAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(MatchCasesAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(BlockAST* ast) {
}

void
StyioASTAnalyzer::typeInfer(MainBlockAST* ast) {
  auto stmts = ast->getStmts();
  for (auto const& s : stmts) {
    s->typeInfer(this);
  }
}
