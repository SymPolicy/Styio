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
StyioAnalyzer::typeInfer(ParamAST* ast) {
}

void
StyioAnalyzer::typeInfer(OptArgAST* ast) {
}

void
StyioAnalyzer::typeInfer(OptKwArgAST* ast) {
}

/*
  The declared type is always the *top* priority
  because the programmer wrote in that way!
*/
void
StyioAnalyzer::typeInfer(FlexBindAST* ast) {
  /* FlexBindAST -> VarAST -> DTypeAST -> StyioDataType */
  auto var_type = ast->getVar()->getDType()->getType();

  /* var type is not declared, try to deduce from the type of value */
  if (var_type.option == StyioDataTypeOption::Undefined) {
    ast->getValue()->typeInfer(this);

    switch (ast->getValue()->getNodeType()) {
      case StyioASTType::Integer: {
        ast->getVar()->setDataType(ast->getValue()->getDataType());
      } break;

      case StyioASTType::Float: {
        ast->getVar()->setDataType(ast->getValue()->getDataType());
      } break;

      case StyioASTType::BinOp: {
        ast->getVar()->setDataType(ast->getValue()->getDataType());
      } break;

      case StyioASTType::Tuple: {
        ast->getVar()->setDataType(ast->getValue()->getDataType());
      } break;

      default:
        break;
    }
  }
  /* if var type has been declared, the type of value must be converted to whatever declared */
  else {
    switch (ast->getValue()->getNodeType()) {
      case StyioASTType::BinOp: {
        static_cast<BinOpAST*>(ast->getValue())->setDType(var_type);
      } break;

      default:
        break;
    }

    ast->getValue()->typeInfer(this);
  }
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
  /* if no element against the consistency, the tuple will have a type. */
  auto elements = ast->getElements();

  bool is_consistent = true;
  StyioDataType aggregated_type = elements[0]->getDataType();
  if (aggregated_type.isUndefined()) {
    for (size_t i = 1; i < elements.size(); i += 1) {
      if (not(elements[i]->getDataType()).equals(aggregated_type)) {
        is_consistent = false;
      }
    }
  }

  if (is_consistent) {
    ast->setConsistency(is_consistent);
    ast->setDataType(aggregated_type);
  }
}

void
StyioAnalyzer::typeInfer(VarTupleAST* ast) {
}

void
StyioAnalyzer::typeInfer(ExtractorAST* ast) {
}

void
StyioAnalyzer::typeInfer(RangeAST* ast) {
}

void
StyioAnalyzer::typeInfer(SetAST* ast) {
}

void
StyioAnalyzer::typeInfer(ListAST* ast) {
  /* if no element against the consistency, the tuple will have a type. */
  auto elements = ast->getElements();

  bool is_consistent = true;
  StyioDataType aggregated_type = elements[0]->getDataType();
  if (aggregated_type.isUndefined()) {
    for (size_t i = 1; i < elements.size(); i += 1) {
      if (not(elements[i]->getDataType()).equals(aggregated_type)) {
        is_consistent = false;
      }
    }
  }

  if (is_consistent) {
    ast->setConsistency(is_consistent);
    ast->setDataType(aggregated_type);
  }
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
  auto lhs = ast->getLHS();
  auto rhs = ast->getRHS();

  if (ast->getType().isUndefined()) {
    lhs->typeInfer(this);
    rhs->typeInfer(this);

    auto op = ast->getOp();
    auto lhs_hint = lhs->getNodeType();
    auto rhs_hint = rhs->getNodeType();

    switch (lhs_hint) {
      case StyioASTType::Integer: {
        switch (rhs_hint) {
          case StyioASTType::Integer: {
            auto lhs_int = static_cast<IntAST*>(lhs);
            auto rhs_int = static_cast<IntAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul) {
              ast->setDType(getMaxType(lhs_int->getDataType(), rhs_int->getDataType()));
            }
            else if (op == StyioOpType::Binary_Div) {
              // 0 / n = 0
              if (std::stoi(lhs_int->getValue()) == 0) {
                ast->setDType(getMaxType(lhs_int->getDataType(), rhs_int->getDataType()));
              }
            }
          } break;

          case StyioASTType::Float: {
            auto lhs_int = static_cast<IntAST*>(lhs);
            auto rhs_float = static_cast<FloatAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul || op == StyioOpType::Binary_Div) {
              ast->setDType(getMaxType(lhs_int->getDataType(), rhs_float->getDataType()));
            }
          } break;

          case StyioASTType::BinOp: {
            auto lhs_expr = static_cast<IntAST*>(lhs);
            auto rhs_expr = static_cast<BinOpAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul || op == StyioOpType::Binary_Div) {
              ast->setDType(getMaxType(lhs_expr->getDataType(), rhs_expr->getDataType()));
            }
          } break;

          default:
            break;
        }
      } break;

      case StyioASTType::Float: {
        switch (rhs_hint) {
          case StyioASTType::Integer: {
            auto lhs_float = static_cast<FloatAST*>(lhs);
            auto rhs_int = static_cast<IntAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul || op == StyioOpType::Binary_Div) {
              ast->setDType(getMaxType(lhs_float->getDataType(), rhs_int->getDataType()));
            }
          } break;

          case StyioASTType::Float: {
            auto lhs_float = static_cast<FloatAST*>(lhs);
            auto rhs_float = static_cast<FloatAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul || op == StyioOpType::Binary_Div) {
              ast->setDType(getMaxType(lhs_float->getDataType(), rhs_float->getDataType()));
            }
          } break;

          default:
            break;
        }
      } break;

      case StyioASTType::BinOp: {
        switch (rhs_hint) {
          case StyioASTType::Integer: {
            auto lhs_expr = static_cast<BinOpAST*>(lhs);
            auto rhs_expr = static_cast<IntAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul || op == StyioOpType::Binary_Div) {
              ast->setDType(getMaxType(lhs_expr->getType(), rhs_expr->getDataType()));
            }
          } break;

          case StyioASTType::Float: {
            auto lhs_binop = static_cast<BinOpAST*>(lhs);
            auto rhs_float = static_cast<FloatAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul || op == StyioOpType::Binary_Div) {
              ast->setDType(getMaxType(lhs_binop->getType(), rhs_float->getDataType()));
            }
          } break;

          case StyioASTType::BinOp: {
            auto lhs_binop = static_cast<BinOpAST*>(lhs);
            auto rhs_binop = static_cast<BinOpAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul || op == StyioOpType::Binary_Div) {
              ast->setDType(getMaxType(lhs_binop->getType(), rhs_binop->getType()));
            }
          } break;

          default:
            break;
        }
      } break;

      default:
        break;
    }
  }
  else {
    /* transfer the type of this binop to the child binop */
    if (lhs->getNodeType() == StyioASTType::BinOp) {
      auto lhs_binop = static_cast<BinOpAST*>(lhs);
      lhs_binop->setDType(ast->getType());
      lhs->typeInfer(this);
    }

    if (rhs->getNodeType() == StyioASTType::BinOp) {
      auto rhs_binop = static_cast<BinOpAST*>(rhs);
      rhs_binop->setDType(ast->getType());
      rhs->typeInfer(this);
    }

    return;
  }
}

void
StyioAnalyzer::typeInfer(FmtStrAST* ast) {
}

void
StyioAnalyzer::typeInfer(ResourceAST* ast) {
}

void
StyioAnalyzer::typeInfer(ResPathAST* ast) {
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
    switch (arg->getNodeType()) {
      case StyioASTType::Integer: {
        arg_types.push_back(static_cast<IntAST*>(arg)->getDataType());
      } break;

      case StyioASTType::Float: {
        arg_types.push_back(static_cast<FloatAST*>(arg)->getDataType());
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
    func_args[i]->setDataType(arg_types[i]);
  }
}

void
StyioAnalyzer::typeInfer(AttrAST* ast) {
}

void
StyioAnalyzer::typeInfer(PrintAST* ast) {
}

void
StyioAnalyzer::typeInfer(ForwardAST* ast) {
}

void
StyioAnalyzer::typeInfer(BackwardAST* ast) {
}

void
StyioAnalyzer::typeInfer(CODPAST* ast) {
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
StyioAnalyzer::typeInfer(FunctionAST* ast) {
  func_defs[ast->getNameAsStr()] = ast;

  if (ast->getForward()->getRetExpr() != nullptr) {
    std::cout << "type infer func ast get ret type" << std::endl;
  }
}

void
StyioAnalyzer::typeInfer(IteratorAST* ast) {
}

void
StyioAnalyzer::typeInfer(InfiniteLoopAST* ast) {
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
