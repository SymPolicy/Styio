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
StyioAnalyzer::typeInfer(CommentAST* ast) {
}

void
StyioAnalyzer::typeInfer(NoneAST* ast) {
}

void
StyioAnalyzer::typeInfer(EmptyAST* ast) {
}

void
StyioAnalyzer::typeInfer(NameAST* ast) {
}

void
StyioAnalyzer::typeInfer(DTypeAST* ast) {
}

void
StyioAnalyzer::typeInfer(BoolAST* ast) {
}

void
StyioAnalyzer::typeInfer(IntAST* ast) {
  if (ast->getType() == StyioDataType::undefined) {
    ast->setType(StyioDataType::i32);
  }
}

void
StyioAnalyzer::typeInfer(FloatAST* ast) {
}

void
StyioAnalyzer::typeInfer(CharAST* ast) {
}

void
StyioAnalyzer::typeInfer(StringAST* ast) {
}

void
StyioAnalyzer::typeInfer(TypeConvertAST*) {
}

void
StyioAnalyzer::typeInfer(VarAST* ast) {
}

void
StyioAnalyzer::typeInfer(ArgAST* ast) {
}

void
StyioAnalyzer::typeInfer(OptArgAST* ast) {
}

void
StyioAnalyzer::typeInfer(OptKwArgAST* ast) {
}

void
StyioAnalyzer::typeInfer(FlexBindAST* ast) {
}

void
StyioAnalyzer::typeInfer(FinalBindAST* ast) {
}

void
StyioAnalyzer::typeInfer(InfiniteAST* ast) {
}

void
StyioAnalyzer::typeInfer(StructAST* ast) {
}

void
StyioAnalyzer::typeInfer(TupleAST* ast) {
}

void
StyioAnalyzer::typeInfer(VarTupleAST* ast) {
}

void
StyioAnalyzer::typeInfer(RangeAST* ast) {
}

void
StyioAnalyzer::typeInfer(SetAST* ast) {
}

void
StyioAnalyzer::typeInfer(ListAST* ast) {
}

void
StyioAnalyzer::typeInfer(SizeOfAST* ast) {
}

void
StyioAnalyzer::typeInfer(ListOpAST* ast) {
}

void
StyioAnalyzer::typeInfer(BinCompAST* ast) {
}

void
StyioAnalyzer::typeInfer(CondAST* ast) {
}

/*
  Int -> Int => Pass
  Int -> Float => Pass

*/
void
StyioAnalyzer::typeInfer(BinOpAST* ast) {
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
StyioAnalyzer::typeInfer(FmtStrAST* ast) {
}

void
StyioAnalyzer::typeInfer(ResourceAST* ast) {
}

void
StyioAnalyzer::typeInfer(LocalPathAST* ast) {
}

void
StyioAnalyzer::typeInfer(RemotePathAST* ast) {
}

void
StyioAnalyzer::typeInfer(WebUrlAST* ast) {
}

void
StyioAnalyzer::typeInfer(DBUrlAST* ast) {
}

void
StyioAnalyzer::typeInfer(ExtPackAST* ast) {
}

void
StyioAnalyzer::typeInfer(ReadFileAST* ast) {
}

void
StyioAnalyzer::typeInfer(EOFAST* ast) {
}

void
StyioAnalyzer::typeInfer(BreakAST* ast) {
}

void
StyioAnalyzer::typeInfer(PassAST* ast) {
}

void
StyioAnalyzer::typeInfer(ReturnAST* ast) {
}

void
StyioAnalyzer::typeInfer(CallAST* ast) {
  if (not func_defs.contains(ast->getNameAsStr())) {
    std::cout << "func " << ast->getNameAsStr() << " not exist" << std::endl;
    return;
  }

  vector<StyioDataType> arg_types;

  for (auto arg : ast->getArgList()) {
    switch (arg->hint()) {
      case StyioNodeHint::Int: {
        arg_types.push_back(static_cast<IntAST*>(arg)->getType());
      } break;

      case StyioNodeHint::Float: {
        arg_types.push_back(static_cast<FloatAST*>(arg)->getType());
      } break;

      default:
        break;
    }
  }

  auto func_args = func_defs[ast->getNameAsStr()]->getAllArgs();

  if (arg_types.size() != func_args.size()) {
    std::cout << "arg list not match" << std::endl;
    return;
  }

  for (size_t i = 0; i < func_args.size(); i++) {
    func_args[i]->setDType(arg_types[i]);
  }
}

void
StyioAnalyzer::typeInfer(PrintAST* ast) {
}

void
StyioAnalyzer::typeInfer(ForwardAST* ast) {
}

void
StyioAnalyzer::typeInfer(CheckEqAST* ast) {
}

void
StyioAnalyzer::typeInfer(CheckIsinAST* ast) {
}

void
StyioAnalyzer::typeInfer(FromToAST* ast) {
}

void
StyioAnalyzer::typeInfer(CondFlowAST* ast) {
}

void
StyioAnalyzer::typeInfer(AnonyFuncAST* ast) {
}

void
StyioAnalyzer::typeInfer(FuncAST* ast) {
  func_defs[ast->getFuncName()] = ast;

  if (ast->getForward()->getRetExpr() != nullptr)
  {
    std::cout << "type infer func ast get ret type" << std::endl;
  }
}

void
StyioAnalyzer::typeInfer(IterAST* ast) {
}

void
StyioAnalyzer::typeInfer(LoopAST* ast) {
}

void
StyioAnalyzer::typeInfer(CasesAST* ast) {
}

void
StyioAnalyzer::typeInfer(MatchCasesAST* ast) {
}

void
StyioAnalyzer::typeInfer(BlockAST* ast) {
}

void
StyioAnalyzer::typeInfer(MainBlockAST* ast) {
  auto stmts = ast->getStmts();
  for (auto const& s : stmts) {
    s->typeInfer(this);
  }
}
