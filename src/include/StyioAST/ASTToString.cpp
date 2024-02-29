// [C++ STL]
#include <memory>
#include <string>
#include <vector>

// [Styio]
#include "../StyioToken/Token.hpp"
#include "../StyioUtil/Util.hpp"
#include "AST.hpp"

using std::string;

string
CommentAST::toString(int indent, bool colorful) {
  return string("Comment { ") + text + " }";
}

string
CommentAST::toStringInline(int indent, bool colorful) {
  return string("Comment { ") + text + " }";
}

string
NoneAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { }");
}

string
NoneAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { }");
}

string
EmptyAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { }");
}

string
EmptyAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { }");
}

string
NameAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ")
         + string("{ ")
         + Id
         + " }";
}

string
NameAST::toStringInline(int indent, bool colorful) {
  return Id;
}

string
DTypeAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ") + string("{ ") + getTypeName() + " }";
}

string
DTypeAST::toStringInline(int indent, bool colorful) {
  return getTypeName();
}

string
BoolAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ")
         + string("{ }");
}

string
BoolAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ")
         + string("{ }");
}

string
IntAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful) + " { " + value + " : " + reprDataType(data_type) + " }";
}

string
IntAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful) + " { " + value + " }";
}

string
FloatAST::toString(int indent, bool colorful) {
  // return reprNodeType(hint(), colorful) + " { " + Significand + " * " +
  // std::to_string(Base) + "^(-" + std::to_string(Exponent) + ")" + " }";
  return reprNodeType(hint(), colorful) + " { " + Value + " }";
}

string
FloatAST::toStringInline(int indent, bool colorful) {
  // return reprNodeType(hint(), colorful) + " { " + Significand + " * " +
  // std::to_string(Base) + "^(-" + std::to_string(Exponent) + ")" + " }";
  return reprNodeType(hint(), colorful) + " { " + value + " }";
}

string
CharAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful) + " { \'" + value + "\' }";
}

string
CharAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful) + " { \'" + value + "\' }";
}

string
StringAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful) + " { \"" + value + "\" }";
}

string
StringAST::toStringInline(int indent, bool colorful) {
  return "\"" + value + "\"";
}

string
TypeConvertAST::toString(int indent, bool colorful) {
  string tStr = "";

  if (PromoType == NumPromoTy::Int_To_Float) {
    tStr = "float";
  }

  return reprNodeType(hint(), colorful, " ") + "{ " + Value->toStringInline() + " ~> " + tStr + " }";
}

string
TypeConvertAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ") + "{ " + Value->toStringInline() + " }";
}

string
VarAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ") + string("{ ") + var_name + " : " + data_type->toStringInline() + " }";
}

string
VarAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ") + string("{ ") + Name + " : " + DType->toStringInline() + " }";
}

string
ArgAST::toString(int indent, bool colorful) {
  std::cout << "arg to string at " << this << " type " << reprDataType(getDType()->getDType()) << std::endl;
  return reprNodeType(hint(), colorful, " ") + string("{ ") + Name + " : " + DType->toStringInline() + " }";
}

string
ArgAST::toStringInline(int indent, bool colorful) {
  std::cout << "arg to string inline at " << this << " type " << reprDataType(getDType()->getDType()) << std::endl;
  return reprNodeType(hint(), colorful, " ") + string("{ ") + Name + " : " + DType->toStringInline() + " }";
}

string
OptArgAST::toString(int indent, bool colorful) {
  return string("arg { ") + Id->toString(indent + 1, colorful) + " }";
}

string
OptArgAST::toStringInline(int indent, bool colorful) {
  return string("arg { ") + Id->toString(indent + 1, colorful) + " }";
}

string
OptKwArgAST::toString(int indent, bool colorful) {
  return string("kwargs { ") + var_name->toString(indent + 1, colorful) + " }";
}

string
OptKwArgAST::toStringInline(int indent, bool colorful) {
  return string("arg { ") + Id->toString(indent + 1, colorful) + " }";
}

string
VarTupleAST::toString(int indent, bool colorful) {
  if (Vars.empty()) {
    return reprNodeType(hint(), colorful, " ")
           + string("{ }");
  }
  else {
    string outstr;

    for (std::vector<VarAST*>::iterator it = Vars.begin();
         it != Vars.end();
         ++it) {
      outstr += make_padding(indent, " ") + (*it)->toString(indent + 1, colorful);
      if (it != (Vars.end() - 1)) {
        outstr += "\n";
      }
    }
    return reprNodeType(hint(), colorful)
           + string(" {\n") + outstr + "}";
  }
}

string
VarTupleAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ")
         + string("{ ") + " }";
}

string
FmtStrAST::toString(int indent, bool colorful) {
  string elemstr = "";
  for (size_t i = 0; i < Fragments.size(); i++) {
    elemstr += make_padding(indent, " ") + "\"" + Fragments.at(i) + "\"\n";
  }

  for (std::vector<StyioAST*>::iterator it = Exprs.begin();
       it != Exprs.end();
       ++it) {
    elemstr += make_padding(indent, " ") + (*it)->toString(indent + 1, colorful);
    if (it != (Exprs.end() - 1)) {
      elemstr += "\n";
    }
  }
  return reprNodeType(hint(), colorful) + " {\n" + elemstr + "}";
}

string
FmtStrAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + " { \"" + "\" }";
}

string
LocalPathAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { ") + Path + string(" }");
}

string
LocalPathAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + Path + string(" }");
}

string
RemotePathAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { ") + Path + string(" }");
}

string
RemotePathAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + Path + string(" }");
}

string
WebUrlAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { ") + Path + string(" }");
}

string
WebUrlAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { ") + Path + string(" }");
}

string
DBUrlAST::toString(
  int indent,
  bool colorful
) {
  return reprNodeType(hint(), colorful)
         + string(" { ") + Path + string(" }");
}

string
DBUrlAST::toStringInline(
  int indent,
  bool colorful
) {
  return reprNodeType(hint(), colorful)
         + string(" { ") + Path + string(" }");
}

string
ListAST::toString(int indent, bool colorful) {
  string ElemStr;

  for (int i = 0; i < Elems.size(); i++) {
    ElemStr += make_padding(indent + 1, " ") + Elems[i]->toString(indent + 1, colorful);
    if (i != (Elems.size() - 1)) {
      ElemStr += "\n";
    }
  };

  return reprNodeType(hint(), colorful)
         + string(" [\n") + ElemStr + "]";
}

string
ListAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
TupleAST::toString(int indent, bool colorful) {
  string ElemStr;

  for (int i = 0; i < Elems.size(); i++) {
    ElemStr += make_padding(indent + 1, " ") + Elems[i]->toString(indent + 1, colorful);
    if (i != (Elems.size() - 1)) {
      ElemStr += "\n";
    }
  }

  return reprNodeType(hint(), colorful)
         + string(" (\n") + ElemStr + ")";
}

string
TupleAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" (") + ")";
}

string
SetAST::toString(int indent, bool colorful) {
  string ElemStr;

  for (int i = 0; i < Elems.size(); i++) {
    ElemStr += make_padding(indent + 1, " ") + Elems[i]->toString(indent + 1, colorful) + "\n";

    if (i != (Elems.size() - 1)) {
      ElemStr += "\n";
    }
  }

  return reprNodeType(hint(), colorful)
         + string(" [\n") + ElemStr + "]";
}

string
SetAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
RangeAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" {\n") + make_padding(indent, " ") + "Start: " + StartVal->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "End  : " + EndVal->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Step : " + StepVal->toString(indent + 1, colorful) + "}";
}

string
RangeAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful) + string(" {\n")
         + make_padding(indent, " ") + "Start: " + StartVal->toString(indent + 1, colorful) + "\n"
         + make_padding(indent, " ") + "End  : " + EndVal->toString(indent + 1, colorful) + "\n"
         + make_padding(indent, " ") + "Step : " + StepVal->toString(indent + 1, colorful)
         + "}";
}

string
SizeOfAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { ") + Value->toStringInline(indent + 1, colorful) + " }";
}

string
SizeOfAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { ") + Value->toStringInline(indent + 1, colorful) + " }";
}

string
BinOpAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ")
         + string("{") + "\n" + make_padding(indent, " ") + "LHS: " + LHS->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "RHS: " + RHS->toString(indent + 1, colorful) + "}";
}

string
BinOpAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful, " ")
         + string("{") + " LHS: " + LHS->toStringInline() + " | RHS: " + RHS->toStringInline() + " }";
}

string
BinCompAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful) + " " + reprToken(CompSign)
         + string(" {\n") + make_padding(indent, " ") + "LHS: " + LhsExpr->toString() + "\n" + make_padding(indent, " ") + "RHS: " + RhsExpr->toString() + "}";
}

string
BinCompAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
CondAST::toString(int indent, bool colorful) {
  if (LogicOp == LogicType::AND || LogicOp == LogicType::OR || LogicOp == LogicType::XOR) {
    return reprNodeType(hint(), colorful) + " {\n" + make_padding(indent, " ") + "Op: " + reprToken(LogicOp) + "\n" + make_padding(indent, " ") + "LHS: " + LhsExpr->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "RHS: " + RhsExpr->toString(indent + 1, colorful) + "}";
  }
  else if (LogicOp == LogicType::NOT) {
    return reprNodeType(hint(), colorful)
           + string(" {\n") + make_padding(indent, " ") + "Op: " + reprToken(LogicOp) + "\n" + make_padding(indent, " ") + "Value: " + ValExpr->toString(indent + 1, colorful) + "}";
  }
  else if (LogicOp == LogicType::RAW) {
    return reprNodeType(hint(), colorful)
           + string(" {\n") + make_padding(indent, " ") + ValExpr->toString(indent + 1, colorful) + "}";
  }
  else {
    return reprNodeType(hint(), colorful) + " { Undefined! }";
  }
}

string
CondAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
CallAST::toString(int indent, bool colorful) {
  string paramStr;

  for (std::vector<StyioAST*>::iterator it = func_args.begin();
       it != func_args.end();
       ++it) {
    paramStr += make_padding(indent, " ");
    paramStr += (*it)->toStringInline();
    if (it != (func_args.end() - 1)) {
      paramStr += "\n";
    }
  }

  return reprNodeType(hint(), colorful, " ") + "{" + "\n" + paramStr + "}";
}

string
CallAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
ListOpAST::toString(int indent, bool colorful) {
  switch (OpType) {
    case StyioNodeHint::Access:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Key: " + Slot1->toString(indent + 1, colorful) + "}";
      break;

    case StyioNodeHint::Access_By_Index:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Index: " + Slot1->toString(indent + 1, colorful) + "}";
      break;
    case StyioNodeHint::Access_By_Name:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Name : " + Slot1->toString(indent + 1, colorful) + "}";
      break;

    case StyioNodeHint::Get_Index_By_Value:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Index: " + Slot1->toString(indent + 1, colorful) + "}";
      break;
    case StyioNodeHint::Get_Indices_By_Many_Values:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Index: " + Slot1->toString(indent + 1, colorful) + "}";
      break;
    case StyioNodeHint::Append_Value:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Value: " + Slot1->toString(indent + 1, colorful) + "}";
      break;
    case StyioNodeHint::Insert_Item_By_Index:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Index: " + Slot1->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Value: " + Slot2->toString(indent + 1, colorful) + "}";
      break;
    case StyioNodeHint::Remove_Item_By_Index:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Index: " + Slot1->toString(indent + 1, colorful) + "}";
      break;
    case StyioNodeHint::Remove_Item_By_Value:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Value: " + Slot1->toString(indent + 1, colorful) + "}";
      break;
    case StyioNodeHint::Remove_Items_By_Many_Indices:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Index: " + Slot1->toString(indent + 1, colorful) + "}";
      break;
    case StyioNodeHint::Remove_Items_By_Many_Values:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Value: " + Slot1->toString(indent + 1, colorful) + "}";
      break;
    case StyioNodeHint::Get_Reversed:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "}";
      break;
    case StyioNodeHint::Get_Index_By_Item_From_Right:
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + TheList->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Index: " + Slot1->toString(indent + 1, colorful) + "}";
      break;

    default:
      return reprNodeType(hint(), colorful)
             + string(" { undefined }");
      break;
  }

  return reprNodeType(hint(), colorful)
         + string(" { undefined }");
}

string
ListOpAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
ResourceAST::toString(int indent, bool colorful) {
  string varStr;

  for (std::vector<StyioAST*>::iterator it = Resources.begin();
       it != Resources.end();
       ++it) {
    varStr += make_padding(indent, " ");
    varStr += (*it)->toStringInline();
    if (it != (Resources.end() - 1)) {
      varStr += "\n";
    }
  }

  return reprNodeType(hint(), colorful)
         + string(" {\n") + varStr + "}";
}

string
ResourceAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
FlexBindAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" {\n") + make_padding(indent, " ") + "Var: " + varName->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Val: " + ValExpr->toString(indent + 1, colorful) + "}";
}

string
FlexBindAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful)
         + string(" { ") + " }";
}

string
FinalBindAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" {\n") + make_padding(indent, " ") + "Var: " + varName->toString(indent) + "\n" + make_padding(indent, " ") + "Val: " + valExpr->toString(indent) + "}";
}

string
FinalBindAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful)
         + string(" { ") + " }";
}

string
StructAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" {") + "}";
}

string
StructAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" {") + "}";
}

string
ReadFileAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" {\n") + make_padding(indent, " ") + "Var: " + varId->toString(indent + 1) + "\n" + make_padding(indent, " ") + "Val: " + valExpr->toString(indent + 1) + "}";
}

string
ReadFileAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
EOFAST::toString(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful)
         + string(" { }");
}

string
EOFAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful)
         + string(" { }");
}

string
BreakAST::toString(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful)
         + string(" { }");
}

string
BreakAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful)
         + string(" { }");
}

string
PassAST::toString(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { }");
}

string
PassAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { }");
}

string
ReturnAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful) + " { " + Expr->toStringInline(indent + 1, colorful) + " }";
}

string
ReturnAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful) + " { " + Expr->toStringInline(indent + 1, colorful) + " }";
}

string
PrintAST::toString(int indent, bool colorful) {
  string outstr;

  for (std::vector<StyioAST*>::iterator it = Exprs.begin();
       it != Exprs.end();
       ++it) {
    outstr += make_padding(indent, " ") + (*it)->toString(indent + 1, colorful);
    if (it != (Exprs.end() - 1)) {
      outstr += "\n";
    }
  }

  return reprNodeType(hint(), colorful)
         + string(" {\n") + outstr + "}";
}

string
PrintAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
ExtPackAST::toString(int indent, bool colorful) {
  string pacPathStr;

  for (int i = 0; i < PackPaths.size(); i++) {
    pacPathStr += make_padding(indent, " ") + PackPaths[i];
    pacPathStr += "\n";
  };

  return reprNodeType(hint(), colorful)
         + string(" {\n") + pacPathStr + "\n} ";
}

string
ExtPackAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
BlockAST::toString(int indent, bool colorful) {
  string stmtStr;

  for (std::vector<StyioAST*>::iterator it = Stmts.begin();
       it != Stmts.end();
       ++it) {
    stmtStr += make_padding(indent, " ") + (*it)->toString(indent + 1, colorful);
    if (it != (Stmts.end() - 1)) {
      stmtStr += "\n";
    }
  }

  return reprNodeType(hint(), colorful)
         + string(" {\n") + stmtStr + "}";
}

string
BlockAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
CasesAST::toString(int indent, bool colorful) {
  string stmtStr = "";

  for (std::vector<std::pair<StyioAST*, StyioAST*>>::iterator it = Cases.begin();
       it != Cases.end();
       ++it) {
    stmtStr += make_padding(indent, " ") + "Left : " + std::get<0>(*it)->toString(indent + 1, colorful) + "\n";
    stmtStr += make_padding(indent, " ") + "Right: " + std::get<1>(*it)->toString(indent + 1, colorful) + "\n";
  }

  return reprNodeType(hint(), colorful)
         + string(" {\n") + stmtStr + make_padding(indent, " ") + "Default: " + LastExpr->toString(indent + 1, colorful)
         + "}";
}

string
CasesAST::toStringInline(
  int indent, bool colorful
) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
CondFlowAST::toString(int indent, bool colorful) {
  if (WhatFlow == StyioNodeHint::CondFlow_True || WhatFlow == StyioNodeHint::CondFlow_False) {
    return reprNodeType(hint(), colorful)
           + string(" {\n") + make_padding(indent, " ") + CondExpr->toStringInline(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Then: " + ThenBlock->toString(indent + 1, colorful) + "}";
  }
  else if (WhatFlow == StyioNodeHint::CondFlow_Both) {
    return reprNodeType(hint(), colorful)
           + string(" {\n") + make_padding(indent, " ") + CondExpr->toStringInline(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Then: " + ThenBlock->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Else: " + ElseBlock->toString(indent + 1, colorful) + "}";
  }
  else {
    return reprNodeType(hint(), colorful)
           + string(" {\n") + make_padding(indent, " ") + CondExpr->toStringInline(indent + 1, colorful) + "}";
  }
}

string
CondFlowAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
CheckEqAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ")
         + string("{ ") + Value->toString(indent + 1, colorful) + " }";
}

string
CheckEqAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ")
         + string("{ ") + Value->toString(indent + 1, colorful) + " }";
}

string
CheckIsinAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" {\n") + make_padding(indent, " ") + Iterable->toString(indent + 1, colorful) + "}";
}

string
CheckIsinAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" {\n") + make_padding(indent, " ") + Iterable->toString(indent + 1, colorful) + "}";
}

string
FromToAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ")
         + string("{") + "\n" + make_padding(indent, " ") + "From: " + FromWhat->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "To: " + ToWhat->toString(indent + 1, colorful) + "}";
}

string
FromToAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ")
         + string("{ ") + " }";
}

string
ForwardAST::toString(int indent, bool colorful) {
  switch (Type) {
    case StyioNodeHint::Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + "Run: " + ThenExpr->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::If_Equal_To_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + ExtraEq->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Run: " + ThenExpr->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::If_Is_In_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + ExtraIsin->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Run: " + ThenExpr->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Cases_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + "Cases: " + ThenExpr->toString(indent + 1, colorful) + "\n" + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::If_True_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + ThenCondFlow->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::If_False_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + ThenCondFlow->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;

    case StyioNodeHint::If_Both_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + ThenCondFlow->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + Params->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Run: " + ThenExpr->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_If_Equal_To_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + Params->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + ExtraEq->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Run: " + ThenExpr->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_If_Is_in_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + Params->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + ExtraIsin->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Run: " + ThenExpr->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_Cases_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + Params->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + "Cases: " + ThenExpr->toString(indent + 1, colorful) + "\n" + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_If_True_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + Params->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + ThenCondFlow->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_If_False_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + Params->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + ThenCondFlow->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;

    case StyioNodeHint::Fill_If_Both_Forward: {
      return reprNodeType(hint(), colorful)
             + string(" {\n") + make_padding(indent, " ") + Params->toString(indent + 1, colorful) + "\n" + make_padding(indent, " ") + ThenCondFlow->toString(indent + 1, colorful) + "}";
    }
    // You should NOT reach this line!
    break;
    default:
      break;
  }
  return reprNodeType(hint(), colorful)
         + string(" { Undefined }");
}

string
ForwardAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful, " ")
         + string("{ ") + " }";
}

string
InfiniteAST::toString(int indent, bool colorful) {
  switch (WhatType) {
    case InfiniteType::Original:
      return reprNodeType(hint(), colorful)
             + string(" { }");
      // You should NOT reach this line!
      break;

    case InfiniteType::Incremental:
      return reprNodeType(hint(), colorful)
             + string(" {") + "\n" + "|" + string(2 * indent, '-') + "| Start: " + Start->toString(indent + 1, colorful) + "\n" + "|" + string(2 * indent, '-') + "| Increment: " + IncEl->toString(indent + 1, colorful) + "\n" + "}";
      // You should NOT reach this line!
      break;

    default:
      break;
  }
  return reprNodeType(hint(), colorful)
         + string(" { Undefined! }");
}

string
InfiniteAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful) + string(" { ") + " }";
}

string
FuncAST::toString(int indent, bool colorful) {
  string suffix = "";

  if (isFinal) {
    suffix = " (Final) ";
  }
  else {
    suffix = " (Flex) ";
  }

  string output = reprNodeType(hint(), colorful, suffix) + "{\n";
  if (Name != nullptr) {
    output += make_padding(indent, " ") + "Name: " + Name->toStringInline(indent + 1, colorful) + "\n";
  }

  if (RetType != nullptr) {
    std::cout << "ret type is not null";
    output += make_padding(indent, " ") + "Type: " + RetType->toStringInline(indent + 1, colorful) + "\n";
  }

  output += make_padding(indent, " ") + Forward->toString(indent + 1, colorful) + "}";
  return output;
}

string
FuncAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" { ") + " }";
}

string
LoopAST::toString(int indent, bool colorful) {
  string output = reprNodeType(hint(), colorful)
                  + string(" {\n") + make_padding(indent, " ") + Forward->toString(indent + 1, colorful) + "}";
  return output;
}

string
LoopAST::toStringInline(int indent, bool colorful) {
  string output = reprNodeType(hint(), colorful)
                  + string(" {\n") + make_padding(indent, " ") + Forward->toString(indent + 1, colorful) + "}";
  return output;
}

string
IterAST::toString(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" {\n") + make_padding(indent, " ") + Collection->toStringInline(indent + 1, colorful) + "}";
}

string
IterAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(hint(), colorful)
         + string(" {\n") + make_padding(indent, " ") + Collection->toStringInline(indent + 1, colorful) + "}";
}

string
MainBlockAST::toString(int indent, bool colorful) {
  string stmtStr;

  if (Stmts.empty())
    return reprNodeType(hint(), colorful, " { }");

  for (std::vector<StyioAST*>::iterator it = Stmts.begin();
       it != Stmts.end();
       ++it) {
    stmtStr += make_padding(indent, " ") + (*it)->toString(indent + 1, colorful);
    if (it != (Stmts.end() - 1)) {
      stmtStr += "\n";
    }
  }

  return reprNodeType(hint(), colorful, " ") + string("{\n") + stmtStr + "}";
}

string
MainBlockAST::toStringInline(int indent, bool colorful) {
  return reprNodeType(this->hint(), colorful, " ") + string("{ ") + " }";
}