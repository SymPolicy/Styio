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
StyioToLLVM::typeInfer(CommentAST* ast) {
}

void
StyioToLLVM::typeInfer(NoneAST* ast) {
}

void
StyioToLLVM::typeInfer(EmptyAST* ast) {
}

void
StyioToLLVM::typeInfer(IdAST* ast) {
}

void
StyioToLLVM::typeInfer(DTypeAST* ast) {
}

void
StyioToLLVM::typeInfer(BoolAST* ast) {
}

void
StyioToLLVM::typeInfer(IntAST* ast) {
  if (ast->getType() == StyioDataType::undefined) {
    ast->setType(StyioDataType::i32);
  }
}

void
StyioToLLVM::typeInfer(FloatAST* ast) {
}

void
StyioToLLVM::typeInfer(CharAST* ast) {
}

void
StyioToLLVM::typeInfer(StringAST* ast) {
}

void
StyioToLLVM::typeInfer(TypeConvertAST*) {
}

void
StyioToLLVM::typeInfer(VarAST* ast) {
}

void
StyioToLLVM::typeInfer(ArgAST* ast) {
}

void
StyioToLLVM::typeInfer(OptArgAST* ast) {
}

void
StyioToLLVM::typeInfer(OptKwArgAST* ast) {
}

void
StyioToLLVM::typeInfer(FlexBindAST* ast) {
}

void
StyioToLLVM::typeInfer(FinalBindAST* ast) {
}

void
StyioToLLVM::typeInfer(InfiniteAST* ast) {
}

void
StyioToLLVM::typeInfer(StructAST* ast) {
}

void
StyioToLLVM::typeInfer(TupleAST* ast) {
}

void
StyioToLLVM::typeInfer(VarTupleAST* ast) {
}

void
StyioToLLVM::typeInfer(RangeAST* ast) {
}

void
StyioToLLVM::typeInfer(SetAST* ast) {
}

void
StyioToLLVM::typeInfer(ListAST* ast) {
}

void
StyioToLLVM::typeInfer(SizeOfAST* ast) {
}

void
StyioToLLVM::typeInfer(ListOpAST* ast) {
}

void
StyioToLLVM::typeInfer(BinCompAST* ast) {
}

void
StyioToLLVM::typeInfer(CondAST* ast) {
}

/*
  Int -> Int => Pass
  Int -> Float => Pass

*/
void
StyioToLLVM::typeInfer(BinOpAST* ast) {
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
StyioToLLVM::typeInfer(FmtStrAST* ast) {
}

void
StyioToLLVM::typeInfer(ResourceAST* ast) {
}

void
StyioToLLVM::typeInfer(LocalPathAST* ast) {
}

void
StyioToLLVM::typeInfer(RemotePathAST* ast) {
}

void
StyioToLLVM::typeInfer(WebUrlAST* ast) {
}

void
StyioToLLVM::typeInfer(DBUrlAST* ast) {
}

void
StyioToLLVM::typeInfer(ExtPackAST* ast) {
}

void
StyioToLLVM::typeInfer(ReadFileAST* ast) {
}

void
StyioToLLVM::typeInfer(EOFAST* ast) {
}

void
StyioToLLVM::typeInfer(BreakAST* ast) {
}

void
StyioToLLVM::typeInfer(PassAST* ast) {
}

void
StyioToLLVM::typeInfer(ReturnAST* ast) {
}

void
StyioToLLVM::typeInfer(CallAST* ast) {
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
StyioToLLVM::typeInfer(PrintAST* ast) {
}

void
StyioToLLVM::typeInfer(ForwardAST* ast) {
}

void
StyioToLLVM::typeInfer(CheckEqAST* ast) {
}

void
StyioToLLVM::typeInfer(CheckIsinAST* ast) {
}

void
StyioToLLVM::typeInfer(FromToAST* ast) {
}

void
StyioToLLVM::typeInfer(CondFlowAST* ast) {
}

void
StyioToLLVM::typeInfer(AnonyFuncAST* ast) {
}

void
StyioToLLVM::typeInfer(FuncAST* ast) {
  func_defs[ast->getFuncName()] = ast;
}

void
StyioToLLVM::typeInfer(IterAST* ast) {
}

void
StyioToLLVM::typeInfer(LoopAST* ast) {
}

void
StyioToLLVM::typeInfer(CasesAST* ast) {
}

void
StyioToLLVM::typeInfer(MatchCasesAST* ast) {
}

void
StyioToLLVM::typeInfer(BlockAST* ast) {
}

void
StyioToLLVM::typeInfer(MainBlockAST* ast) {
  auto stmts = ast->getStmts();
  for (auto const& s : stmts) {
    s->typeInfer(this);
  }
}
