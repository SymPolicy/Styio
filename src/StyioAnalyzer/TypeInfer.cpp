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
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "Util.hpp"

static std::vector<ParamAST*>
params_of_func_def(StyioAST* def) {
  if (auto* f = dynamic_cast<FunctionAST*>(def)) {
    return f->params;
  }
  if (auto* s = dynamic_cast<SimpleFuncAST*>(def)) {
    return s->params;
  }
  return {};
}

namespace {

StyioDataType const kStringType{
  StyioDataTypeOption::String, "string", 0};

StyioDataType
infer_collection_elem_type(StyioAST* coll) {
  if (auto* L = dynamic_cast<ListAST*>(coll)) {
    auto const& els = L->getElements();
    if (els.empty()) {
      return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
    }
    if (els[0]->getNodeType() == StyioNodeType::String) {
      return kStringType;
    }
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  }
  if (coll->getNodeType() == StyioNodeType::FileResource
      || coll->getNodeType() == StyioNodeType::Id) {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  }
  return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
}

bool
type_is_string(StyioDataType const& t) {
  return t.option == StyioDataTypeOption::String;
}

bool
type_is_intish(StyioDataType const& t) {
  return t.option == StyioDataTypeOption::Integer
    || t.option == StyioDataTypeOption::Float;
}

std::optional<bool>
expr_is_string_hint(StyioAnalyzer* an, StyioAST* x) {
  switch (x->getNodeType()) {
    case StyioNodeType::String:
      return true;
    case StyioNodeType::Integer:
    case StyioNodeType::Float:
      return false;
    case StyioNodeType::Id: {
      auto* nm = static_cast<NameAST*>(x);
      auto it = an->local_binding_types.find(nm->getAsStr());
      if (it == an->local_binding_types.end()) {
        return std::nullopt;
      }
      return type_is_string(it->second);
    }
    case StyioNodeType::BinOp: {
      StyioDataType t = static_cast<BinOpAST*>(x)->getType();
      if (t.isUndefined()) {
        return std::nullopt;
      }
      return type_is_string(t);
    }
    default:
      return std::nullopt;
  }
}

std::optional<bool>
expr_is_intish_hint(StyioAnalyzer* an, StyioAST* x) {
  switch (x->getNodeType()) {
    case StyioNodeType::Integer:
    case StyioNodeType::Float:
      return true;
    case StyioNodeType::String:
      return false;
    case StyioNodeType::Id: {
      auto* nm = static_cast<NameAST*>(x);
      auto it = an->local_binding_types.find(nm->getAsStr());
      if (it == an->local_binding_types.end()) {
        return std::nullopt;
      }
      return type_is_intish(it->second);
    }
    case StyioNodeType::BinOp: {
      StyioDataType t = static_cast<BinOpAST*>(x)->getType();
      if (t.isUndefined()) {
        return std::nullopt;
      }
      return !type_is_string(t);
    }
    default:
      return std::nullopt;
  }
}

bool
infer_concat_string_add(StyioAnalyzer* an, BinOpAST* ast, StyioAST* lhs, StyioAST* rhs) {
  std::optional<bool> ls = expr_is_string_hint(an, lhs);
  std::optional<bool> rs = expr_is_string_hint(an, rhs);
  std::optional<bool> li = expr_is_intish_hint(an, lhs);
  std::optional<bool> ri = expr_is_intish_hint(an, rhs);
  if ((ls && *ls) || (rs && *rs)) {
    if ((ls && *ls && rs && *rs) || (ls && *ls && ri && *ri) || (rs && *rs && li && *li)) {
      ast->setDType(kStringType);
      return true;
    }
  }
  return false;
}

}  // namespace

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
StyioAnalyzer::typeInfer(TypeAST* ast) {
}

void
StyioAnalyzer::typeInfer(TypeTupleAST* ast) {
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
  const std::string& bound_name = ast->getNameAsStr();
  if (fixed_assignment_names_.count(bound_name) != 0) {
    throw StyioSyntaxError(
      "variable `" + bound_name + "` was defined with `:=` (fixed assignment); "
      "cannot reassign with `=` (flex bind). Use a different name.");
  }

  auto var_type = ast->getVar()->getDType()->type;

  if (var_type.option != StyioDataTypeOption::Undefined) {
    if (ast->getValue()->getNodeType() == StyioNodeType::BinOp) {
      static_cast<BinOpAST*>(ast->getValue())->setDType(var_type);
    }
  }

  ast->getValue()->typeInfer(this);

  if (var_type.option == StyioDataTypeOption::Undefined) {
    switch (ast->getValue()->getNodeType()) {
      case StyioNodeType::Integer: {
        ast->getVar()->setDataType(static_cast<IntAST*>(ast->getValue())->getDataType());
      } break;

      case StyioNodeType::Float: {
        ast->getVar()->setDataType(static_cast<FloatAST*>(ast->getValue())->getDataType());
      } break;

      case StyioNodeType::BinOp: {
        ast->getVar()->setDataType(static_cast<BinOpAST*>(ast->getValue())->getType());
      } break;

      case StyioNodeType::Bool:
      case StyioNodeType::Condition:
      case StyioNodeType::Compare: {
        ast->getVar()->setDataType(StyioDataType{StyioDataTypeOption::Bool, "bool", 1});
      } break;

      case StyioNodeType::Tuple: {
        ast->getVar()->setDataType(ast->getValue()->getDataType());
      } break;

      default:
        break;
    }
  }

  local_binding_types[ast->getNameAsStr()] = ast->getVar()->getDType()->type;
}

void
StyioAnalyzer::typeInfer(FinalBindAST* ast) {
  ast->getValue()->typeInfer(this);
  auto vt = ast->getVar()->getDType()->type;
  if (ast->getValue()->getNodeType() == StyioNodeType::BinOp) {
    static_cast<BinOpAST*>(ast->getValue())->setDType(vt);
    ast->getValue()->typeInfer(this);
  }
  local_binding_types[ast->getVar()->getNameAsStr()] = vt;
  fixed_assignment_names_.insert(ast->getVar()->getNameAsStr());
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
  ast->getLHS()->typeInfer(this);
  ast->getRHS()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(CondAST* ast) {
  if (ast->getValue()) {
    ast->getValue()->typeInfer(this);
  }
  if (ast->getLHS()) {
    ast->getLHS()->typeInfer(this);
  }
  if (ast->getRHS()) {
    ast->getRHS()->typeInfer(this);
  }
}

void
StyioAnalyzer::typeInfer(UndefinedLitAST* ast) {
  (void)ast;
}

void
StyioAnalyzer::typeInfer(WaveMergeAST* ast) {
  ast->getCond()->typeInfer(this);
  ast->getTrueVal()->typeInfer(this);
  ast->getFalseVal()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(WaveDispatchAST* ast) {
  ast->getCond()->typeInfer(this);
  ast->getTrueArm()->typeInfer(this);
  ast->getFalseArm()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(FallbackAST* ast) {
  ast->getPrimary()->typeInfer(this);
  ast->getAlternate()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(GuardSelectorAST* ast) {
  ast->getBase()->typeInfer(this);
  ast->getCond()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(EqProbeAST* ast) {
  ast->getBase()->typeInfer(this);
  ast->getProbeValue()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(FileResourceAST* ast) {
  ast->getPath()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(StdStreamAST* ast) {
  /* No children to infer. */
}

void
StyioAnalyzer::typeInfer(HandleAcquireAST* ast) {
  ast->getVar()->typeInfer(this);
  ast->getResource()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(ResourceWriteAST* ast) {
  ast->getData()->typeInfer(this);
  ast->getResource()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(ResourceRedirectAST* ast) {
  ast->getData()->typeInfer(this);
  ast->getResource()->typeInfer(this);
}

/*
  Int -> Int => Pass
  Int -> Float => Pass
*/
void
StyioAnalyzer::typeInfer(BinOpAST* ast) {
  auto lhs = ast->getLHS();
  auto rhs = ast->getRHS();
  auto op = ast->getOp();

  if (op == StyioOpType::Self_Add_Assign || op == StyioOpType::Self_Sub_Assign
      || op == StyioOpType::Self_Mul_Assign || op == StyioOpType::Self_Div_Assign
      || op == StyioOpType::Self_Mod_Assign) {
    rhs->typeInfer(this);
    auto* nm = static_cast<NameAST*>(lhs);
    auto it = local_binding_types.find(nm->getAsStr());
    if (it != local_binding_types.end()) {
      ast->setDType(it->second);
    }
    else {
      ast->setDType(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
    }
    return;
  }

  if (ast->getType().isUndefined()) {
    lhs->typeInfer(this);
    rhs->typeInfer(this);
    if (op == StyioOpType::Binary_Add
        && infer_concat_string_add(this, ast, lhs, rhs)) {
      return;
    }
    auto lhs_hint = lhs->getNodeType();
    auto rhs_hint = rhs->getNodeType();

    switch (lhs_hint) {
      case StyioNodeType::Integer: {
        switch (rhs_hint) {
          case StyioNodeType::Integer: {
            auto lhs_int = static_cast<IntAST*>(lhs);
            auto rhs_int = static_cast<IntAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_int->getDataType(), rhs_int->getDataType()));
            }
          } break;

          case StyioNodeType::Float: {
            auto lhs_int = static_cast<IntAST*>(lhs);
            auto rhs_float = static_cast<FloatAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_int->getDataType(), rhs_float->getDataType()));
            }
          } break;

          case StyioNodeType::BinOp: {
            auto lhs_expr = static_cast<IntAST*>(lhs);
            auto rhs_expr = static_cast<BinOpAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_expr->getDataType(), rhs_expr->getType()));
            }
          } break;

          default:
            break;
        }
      } break;

      case StyioNodeType::Float: {
        switch (rhs_hint) {
          case StyioNodeType::Integer: {
            auto lhs_float = static_cast<FloatAST*>(lhs);
            auto rhs_int = static_cast<IntAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_float->getDataType(), rhs_int->getDataType()));
            }
          } break;

          case StyioNodeType::Float: {
            auto lhs_float = static_cast<FloatAST*>(lhs);
            auto rhs_float = static_cast<FloatAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_float->getDataType(), rhs_float->getDataType()));
            }
          } break;

          default:
            break;
        }
      } break;

      case StyioNodeType::BinOp: {
        switch (rhs_hint) {
          case StyioNodeType::Integer: {
            auto lhs_expr = static_cast<BinOpAST*>(lhs);
            auto rhs_expr = static_cast<IntAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_expr->getType(), rhs_expr->getDataType()));
            }
          } break;

          case StyioNodeType::Float: {
            auto lhs_binop = static_cast<BinOpAST*>(lhs);
            auto rhs_float = static_cast<FloatAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_binop->getType(), rhs_float->getDataType()));
            }
          } break;

          case StyioNodeType::BinOp: {
            auto lhs_binop = static_cast<BinOpAST*>(lhs);
            auto rhs_binop = static_cast<BinOpAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_binop->getType(), rhs_binop->getType()));
            }
          } break;

          default:
            break;
        }
      } break;

      case StyioNodeType::Id: {
        auto* lid = static_cast<NameAST*>(lhs);
        auto lt_it = local_binding_types.find(lid->getAsStr());
        if (lt_it == local_binding_types.end()) {
          break;
        }
        StyioDataType lt = lt_it->second;

        switch (rhs_hint) {
          case StyioNodeType::Integer: {
            auto* ri = static_cast<IntAST*>(rhs);
            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lt, ri->getDataType()));
            }
          } break;

          case StyioNodeType::Float: {
            auto* rf = static_cast<FloatAST*>(rhs);
            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lt, rf->getDataType()));
            }
          } break;

          case StyioNodeType::BinOp: {
            auto* rb = static_cast<BinOpAST*>(rhs);
            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lt, rb->getType()));
            }
          } break;

          case StyioNodeType::Id: {
            auto* rid = static_cast<NameAST*>(rhs);
            auto rt_it = local_binding_types.find(rid->getAsStr());
            if (rt_it == local_binding_types.end()) {
              break;
            }
            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lt, rt_it->second));
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
    if (lhs->getNodeType() == StyioNodeType::BinOp) {
      auto lhs_binop = static_cast<BinOpAST*>(lhs);
      lhs_binop->setDType(ast->getType());
      lhs->typeInfer(this);
    }

    if (rhs->getNodeType() == StyioNodeType::BinOp) {
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
StyioAnalyzer::typeInfer(ContinueAST* ast) {
  (void)ast;
}

void
StyioAnalyzer::typeInfer(PassAST* ast) {
}

void
StyioAnalyzer::typeInfer(ReturnAST* ast) {
}

void
StyioAnalyzer::typeInfer(FuncCallAST* ast) {
  if (not func_defs.contains(ast->getNameAsStr())) {
    return;
  }

  vector<StyioDataType> arg_types;

  for (auto arg : ast->getArgList()) {
    switch (arg->getNodeType()) {
      case StyioNodeType::Integer: {
        arg_types.push_back(static_cast<IntAST*>(arg)->getDataType());
      } break;

      case StyioNodeType::Float: {
        arg_types.push_back(static_cast<FloatAST*>(arg)->getDataType());
      } break;

      case StyioNodeType::String: {
        arg_types.push_back(static_cast<StringAST*>(arg)->getDataType());
      } break;

      case StyioNodeType::Id: {
        auto* nm = static_cast<NameAST*>(arg);
        auto it = local_binding_types.find(nm->getAsStr());
        if (it != local_binding_types.end()) {
          arg_types.push_back(it->second);
        }
        else {
          arg_types.push_back(
            StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
        }
      } break;

      default:
        break;
    }
  }

  auto func_args = params_of_func_def(func_defs[ast->getNameAsStr()]);

  if (arg_types.size() != func_args.size()) {
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
  for (auto* e : ast->exprs) {
    e->typeInfer(this);
  }
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
StyioAnalyzer::typeInfer(CheckEqualAST* ast) {
}

void
StyioAnalyzer::typeInfer(CheckIsinAST* ast) {
}

void
StyioAnalyzer::typeInfer(HashTagNameAST* ast) {
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
}

void
StyioAnalyzer::typeInfer(SimpleFuncAST* ast) {
  func_defs[ast->func_name->getAsStr()] = ast;
}

void
StyioAnalyzer::typeInfer(IteratorAST* ast) {
  auto saved = local_binding_types;
  auto saved_fixed = fixed_assignment_names_;
  ast->collection->typeInfer(this);
  StyioDataType et = infer_collection_elem_type(ast->collection);
  if (!ast->params.empty()) {
    local_binding_types[ast->params[0]->getNameAsStr()] = et;
  }
  for (auto* f : ast->following) {
    f->typeInfer(this);
  }
  local_binding_types = std::move(saved);
  fixed_assignment_names_ = std::move(saved_fixed);
}

void
StyioAnalyzer::typeInfer(StreamZipAST* ast) {
  auto saved = local_binding_types;
  auto saved_fixed = fixed_assignment_names_;
  ast->getCollectionA()->typeInfer(this);
  ast->getCollectionB()->typeInfer(this);
  StyioDataType ea = infer_collection_elem_type(ast->getCollectionA());
  StyioDataType eb = infer_collection_elem_type(ast->getCollectionB());
  if (!ast->getParamsA().empty()) {
    local_binding_types[ast->getParamsA()[0]->getNameAsStr()] = ea;
  }
  if (!ast->getParamsB().empty()) {
    local_binding_types[ast->getParamsB()[0]->getNameAsStr()] = eb;
  }
  for (auto* f : ast->getFollowing()) {
    f->typeInfer(this);
  }
  local_binding_types = std::move(saved);
  fixed_assignment_names_ = std::move(saved_fixed);
}

void
StyioAnalyzer::typeInfer(SnapshotDeclAST* ast) {
  snapshot_var_names_.insert(ast->getVar()->getAsStr());
  local_binding_types[ast->getVar()->getAsStr()] =
    StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  ast->getResource()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(InstantPullAST* ast) {
  ast->getResource()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(IterSeqAST* ast) {
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
  for (auto* s : ast->stmts) {
    s->typeInfer(this);
  }
}

void
StyioAnalyzer::typeInfer(StateDeclAST* ast) {
  if (ast->getAccInit()) {
    ast->getAccInit()->typeInfer(this);
  }
  ast->getUpdateExpr()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(StateRefAST* ast) {
  (void)ast;
}

void
StyioAnalyzer::typeInfer(HistoryProbeAST* ast) {
  ast->getDepth()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(SeriesIntrinsicAST* ast) {
  ast->getBase()->typeInfer(this);
  ast->getWindow()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(MainBlockAST* ast) {
  snapshot_var_names_.clear();
  local_binding_types.clear();
  fixed_assignment_names_.clear();
  auto stmts = ast->getStmts();
  for (auto const& s : stmts) {
    s->typeInfer(this);
  }
}
