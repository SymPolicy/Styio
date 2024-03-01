#include "../StyioAST/AST.hpp"

// [C++ STL]
#include <iostream>
#include <string>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioUtil/Util.hpp"

std::string
StyioAnalyzer::toString(CommentAST* ast, int indent) {
  return string("Comment { ") + ast->getText() + " }";
}

std::string
StyioAnalyzer::toString(NoneAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ }";
}

std::string
StyioAnalyzer::toString(EmptyAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ }";
}

std::string
StyioAnalyzer::toString(NameAST* ast, int indent) {
  return reprNodeType(ast->hint()) + " { " + ast->getNameAsStr() + " }";
}

std::string
StyioAnalyzer::toString(DTypeAST* ast, int indent) {
  return reprNodeType(ast->hint()) + " { " + ast->getTypeName() + " }";
}

std::string
StyioAnalyzer::toString(BoolAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + string("{ }");
}

std::string
StyioAnalyzer::toString(IntAST* ast, int indent) {
  return "{ " + ast->getValue() + " : " + reprDataType(ast->getType()) + " }";
}

std::string
StyioAnalyzer::toString(FloatAST* ast, int indent) {
  return "{ " + ast->getValue() + " : " + reprDataType(ast->getType()) + " }";
}

std::string
StyioAnalyzer::toString(CharAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ \'" + ast->getValue() + "\' }";
}

std::string
StyioAnalyzer::toString(StringAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ \"" + ast->getValue() + "\" }";
}

std::string
StyioAnalyzer::toString(TypeConvertAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ " + " }";
}

std::string
StyioAnalyzer::toString(VarAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") 
         + string("{ ") + ast->getNameAsStr() + " : " + ast->getType()->getTypeName() + " }";
}

std::string
StyioAnalyzer::toString(ArgAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") 
         + string("{ ") + ast->getName() + " : " + ast->getDType()->getTypeName() + " }";
}

std::string
StyioAnalyzer::toString(OptArgAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ")
         + "{ " + " }";
}

std::string
StyioAnalyzer::toString(OptKwArgAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ")
         + "{ " + " }";
}

std::string
StyioAnalyzer::toString(FlexBindAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + string("{")
         + "\n" + make_padding(indent, " ") + ast->getVar()->toString(this)
         + "\n" + make_padding(indent, " ") + "val = " + ast->getValue()->toString(this, indent + 1)
         + "}";
}

std::string
StyioAnalyzer::toString(FinalBindAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + string("{")
         + "\n" + make_padding(indent, " ") + "Var: " + ast->getVarName()->toString(this, indent + 1)
         + "\n" + make_padding(indent, " ") + "Val: " + ast->getValue()->toString(this, indent + 1)
         + "}";
}

std::string
StyioAnalyzer::toString(InfiniteAST* ast, int indent) {
  switch (ast->getType()) {
    case InfiniteType::Original: {
      return reprNodeType(ast->hint(), " ") + string("{ }");
    } break;  // You should NOT reach this line!

    case InfiniteType::Incremental: {
      return reprNodeType(ast->hint(), " ") + string("{")
             + "\n" + "|" + string(2 * indent, '-') + "| Start: " + ast->getStart()->toString(this, indent + 1)
             + "\n" + "|" + string(2 * indent, '-') + "| Increment: " + ast->getIncEl()->toString(this, indent + 1)
             + "}";

    } break;  // You should NOT reach this line!

    default:
      break;
  }

  return reprNodeType(ast->hint(), " ") + string("{ Undefined! }");
}

std::string
StyioAnalyzer::toString(StructAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{" + "}";
}

std::string
StyioAnalyzer::toString(TupleAST* ast, int indent) {
  string ElemStr;

  auto Elems = ast->getElements();
  for (int i = 0; i < Elems.size(); i++) {
    ElemStr += make_padding(indent + 1) + Elems[i]->toString(this, indent + 1);
    if (i < (Elems.size() - 1)) {
      ElemStr += "\n";
    }
  }

  return reprNodeType(ast->hint(), " ") + "(\n" + ElemStr + ")";
}

std::string
StyioAnalyzer::toString(VarTupleAST* ast, int indent) {
  auto Vars = ast->getParams();
  if (Vars.empty()) {
    return reprNodeType(ast->hint(), " ") + "[ ]";
  }
  else {
    string varStr;

    for (int i = 0; i < Vars.size(); i++) {
      varStr += make_padding(indent, " ") + Vars[i]->toString(this, indent + 1);
      if (i < (Vars.size() - 1)) {
        varStr += "\n";
      }
    }

    return reprNodeType(ast->hint(), " ") + "[\n" + varStr + "]";
  }
}

std::string
StyioAnalyzer::toString(RangeAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{\n"
         + make_padding(indent, " ") + "Start : " + ast->getStart()->toString(this, indent + 1) + "\n"
         + make_padding(indent, " ") + "End   : " + ast->getEnd()->toString(this, indent + 1) + "\n"
         + make_padding(indent, " ") + "Step  : " + ast->getStep()->toString(this, indent + 1)
         + "}";
}

std::string
StyioAnalyzer::toString(SetAST* ast, int indent) {
  string ElemStr;

  auto Elems = ast->getElements();
  for (int i = 0; i < Elems.size(); i++) {
    ElemStr += make_padding(indent + 1) + Elems[i]->toString(this, indent + 1);
    if (i != (Elems.size() - 1)) {
      ElemStr += "\n";
    }
  }

  return reprNodeType(ast->hint(), " ") + "(\n" + ElemStr + ")";
}

std::string
StyioAnalyzer::toString(ListAST* ast, int indent) {
  string ElemStr;

  auto Elems = ast->getElements();
  for (int i = 0; i < Elems.size(); i++) {
    ElemStr += make_padding(indent + 1) + Elems[i]->toString(this, indent + 1);
    if (i != (Elems.size() - 1)) {
      ElemStr += "\n";
    }
  }

  return reprNodeType(ast->hint(), " ") + "(\n" + ElemStr + ")";
}

std::string
StyioAnalyzer::toString(SizeOfAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ")
         + "{ " + ast->getValue()->toString(this, indent + 1) + " }";
}

std::string
StyioAnalyzer::toString(ListOpAST* ast, int indent) {
  auto OpType = ast->getOp();
  switch (OpType) {
    case StyioNodeHint::Access:
      return reprNodeType(ast->hint(), " ") + "{"
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Key: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;

    case StyioNodeHint::Access_By_Index:
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeHint::Access_By_Name:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Name : " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;

    case StyioNodeHint::Get_Index_By_Value:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeHint::Get_Indices_By_Many_Values:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeHint::Append_Value:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Value: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeHint::Insert_Item_By_Index:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Value: " + ast->getSlot2()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeHint::Remove_Item_By_Index:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeHint::Remove_Item_By_Value:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Value: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeHint::Remove_Items_By_Many_Indices:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeHint::Remove_Items_By_Many_Values:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Value: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeHint::Get_Reversed:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeHint::Get_Index_By_Item_From_Right:
      return reprNodeType(ast->hint(), " ")
             + "\n" + make_padding(indent, " ") + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;

    default: {
      return reprNodeType(ast->hint(), " ") + string("{ undefined }");
    } break;
  }

  return reprNodeType(ast->hint(), " ") + string("{ undefined }");
}

std::string
StyioAnalyzer::toString(BinCompAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + reprToken(ast->getSign()) + " {\n"
         + make_padding(indent, " ") + "LHS: " + ast->getLHS()->toString(this, indent + 1) + "\n"
         + make_padding(indent, " ") + "RHS: " + ast->getRHS()->toString(this, indent + 1)
         + "}";
}

std::string
StyioAnalyzer::toString(CondAST* ast, int indent) {
  LogicType LogicOp = ast->getSign();

  if (LogicOp == LogicType::AND || LogicOp == LogicType::OR || LogicOp == LogicType::XOR) {
    return reprNodeType(ast->hint(), " ") + "{\n"
           + make_padding(indent, " ") + "Op: " + reprToken(LogicOp) + "\n"
           + make_padding(indent, " ") + "LHS: " + ast->getLHS()->toString(this, indent + 1) + "\n"
           + make_padding(indent, " ") + "RHS: " + ast->getRHS()->toString(this, indent + 1)
           + "}";
  }
  else if (LogicOp == LogicType::NOT) {
    return reprNodeType(ast->hint(), " ") + "\n"
           + make_padding(indent, " ") + "Op: " + reprToken(LogicOp) + "\n"
           + make_padding(indent, " ") + "Value: " + ast->getValue()->toString(this, indent + 1)
           + "}";
  }
  else if (LogicOp == LogicType::RAW) {
    return reprNodeType(ast->hint(), " ") + "\n"
           + make_padding(indent, " ") + ast->getValue()->toString(this, indent + 1) + "}";
  }
  else {
    return reprNodeType(ast->hint(), " ") + " { Undefined! }";
  }
}

/*
  Int -> Int => Pass
  Int -> Float => Pass

*/
std::string
StyioAnalyzer::toString(BinOpAST* ast, int indent) {
  return reprNodeType(ast->hint(), " : ") + reprDataType(ast->getType()) + " {" + "\n"
         + make_padding(indent, " ") + "LHS : " + ast->getLHS()->toString(this, indent + 1) + "\n"
         + make_padding(indent, " ") + "OP  : " + reprToken(ast->getOp()) + "\n"
         + make_padding(indent, " ") + "RHS : " + ast->getRHS()->toString(this, indent + 1)
         + "}";
}

std::string
StyioAnalyzer::toString(FmtStrAST* ast, int indent) {
  auto Fragments = ast->getFragments();
  auto Exprs = ast->getExprs();

  string elemstr = "";
  for (size_t i = 0; i < Fragments.size(); i++) {
    elemstr += make_padding(indent, " ") + "\"" + Fragments[i] + "\"\n";
  }

  for (int i = 0; i < Exprs.size(); i++) {
    elemstr += make_padding(indent, " ") + Exprs[i]->toString(this, indent + 1);
    if (i < (Exprs.size() - 1)) {
      elemstr += "\n";
    }
  }

  return reprNodeType(ast->hint(), " ") + "{\n" + elemstr + "}";
}

std::string
StyioAnalyzer::toString(ResourceAST* ast, int indent) {
  string varStr;

  auto Resources = ast->getResList();
  for (int i = 0; i < Resources.size(); i++) {
    varStr += make_padding(indent, " ") + Resources[i]->toString(this, indent + 1);
    if (i < (Resources.size() - 1)) {
      varStr += "\n";
    }
  }

  return reprNodeType(ast->hint(), " ") + "{\n" + varStr + "}";
}

std::string
StyioAnalyzer::toString(LocalPathAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ " + ast->getPath() + " }";
}

std::string
StyioAnalyzer::toString(RemotePathAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ " + ast->getPath() + " }";
}

std::string
StyioAnalyzer::toString(WebUrlAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ " + ast->getPath() + " }";
}

std::string
StyioAnalyzer::toString(DBUrlAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ " + ast->getPath() + " }";
}

std::string
StyioAnalyzer::toString(ExtPackAST* ast, int indent) {
  string pathStr;

  auto PackPaths = ast->getPaths();
  for (int i = 0; i < PackPaths.size(); i++) {
    pathStr += make_padding(indent, " ") + PackPaths[i] + "\n";
  };

  return reprNodeType(ast->hint(), " ") + "{\n" + pathStr + "\n}";
}

std::string
StyioAnalyzer::toString(ReadFileAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{\n"
         + make_padding(indent, " ") + "Var: " + ast->getId()->toString(this, indent + 1) + "\n"
         + make_padding(indent, " ") + "Val: " + ast->getValue()->toString(this, indent + 1) + "}";
}

std::string
StyioAnalyzer::toString(EOFAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + string("{ }");
}

std::string
StyioAnalyzer::toString(BreakAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + string("{ }");
}

std::string
StyioAnalyzer::toString(PassAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + string("{ }");
}

std::string
StyioAnalyzer::toString(ReturnAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{\n"
         + make_padding(indent, " ") + ast->getExpr()->toString(this, indent + 1)
         + "}";
}

std::string
StyioAnalyzer::toString(CallAST* ast, int indent) {
  string outstr;

  auto call_args = ast->getArgList();
  for (int i = 0; i < call_args.size(); i++) {
    outstr += make_padding(indent, " ") + call_args[i]->toString(this, indent + 1);
    if (i < (call_args.size() - 1)) {
      outstr += "\n";
    }
  }

  return reprNodeType(ast->hint(), " ") + ast->getFuncName()->toString(this) + " [\n" + outstr + "]";
}

std::string
StyioAnalyzer::toString(PrintAST* ast, int indent) {
  string outstr;

  auto Exprs = ast->getExprs();
  for (int i = 0; i < Exprs.size(); i++) {
    outstr += make_padding(indent, " ") + Exprs[i]->toString(this, indent + 1);
    if (i < (Exprs.size() - 1)) {
      outstr += "\n";
    }
  }

  return reprNodeType(ast->hint(), " ") + string("{\n") + outstr + "}";
}

std::string
StyioAnalyzer::toString(ForwardAST* ast, int indent) {
  switch (ast->hint()) {
    case StyioNodeHint::Forward: {
      return reprNodeType(ast->hint(), " ") + "{\n"
             + make_padding(indent, " ") + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::If_Equal_To_Forward: {
      return reprNodeType(ast->hint(), " ") + "{\n"
             + make_padding(indent, " ") + ast->getCheckEq()->toString(this, indent + 1) + "\n"
             + make_padding(indent, " ") + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::If_Is_In_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getCheckIsin()->toString(this, indent + 1) + "\n"
             + make_padding(indent, " ") + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Cases_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + "Cases: " + ast->getThen()->toString(this, indent + 1) + "\n"
             + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::If_True_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::If_False_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;

    case StyioNodeHint::If_Both_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent, " ") + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_If_Equal_To_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent, " ") + ast->getCheckEq()->toString(this, indent + 1) + "\n"
             + make_padding(indent, " ") + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_If_Is_in_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent, " ") + ast->getCheckIsin()->toString(this, indent + 1) + "\n"
             + make_padding(indent, " ") + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_Cases_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getVarTuple()->toString(this, indent + 1)
             + "\n" + make_padding(indent, " ") + "Cases: " + ast->getThen()->toString(this, indent + 1)
             + "\n" + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_If_True_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent, " ") + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioNodeHint::Fill_If_False_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent, " ") + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;

    case StyioNodeHint::Fill_If_Both_Forward: {
      return reprNodeType(ast->hint(), " ")
             + "{\n" + make_padding(indent, " ") + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent, " ") + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    default:
      break;
  }
  return reprNodeType(ast->hint(), " ") + string("{ Undefined }");
}

std::string
StyioAnalyzer::toString(CheckEqAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + string("{ ")
         + ast->getValue()->toString(this, indent + 1) + " }";
}

std::string
StyioAnalyzer::toString(CheckIsinAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + string("{\n")
         + make_padding(indent, " ") + ast->getIterable()->toString(this, indent + 1) + "}";
}

std::string
StyioAnalyzer::toString(FromToAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + string("{") + "\n"
         + make_padding(indent, " ") + "From: " + ast->getFromExpr()->toString(this, indent + 1) + "\n"
         + make_padding(indent, " ") + "To: " + ast->getToExpr()->toString(this, indent + 1) + "}";
}

std::string
StyioAnalyzer::toString(CondFlowAST* ast, int indent) {
  auto WhatFlow = ast->hint();

  if (WhatFlow == StyioNodeHint::CondFlow_True || WhatFlow == StyioNodeHint::CondFlow_False) {
    return reprNodeType(ast->hint(), " ")
           + string("{\n")
           + make_padding(indent, " ") + ast->getCond()->toString(this, indent + 1) + "\n"
           + make_padding(indent, " ") + "Then: " + ast->getThen()->toString(this, indent + 1) + "}";
  }
  else if (WhatFlow == StyioNodeHint::CondFlow_Both) {
    return reprNodeType(ast->hint(), " ")
           + string("{\n") + make_padding(indent, " ") + ast->getCond()->toString(this, indent + 1) + "\n"
           + make_padding(indent, " ") + "Then: " + ast->getThen()->toString(this, indent + 1) + "\n"
           + make_padding(indent, " ") + "Else: " + ast->getElse()->toString(this, indent + 1) + "}";
  }
  else {
    return reprNodeType(ast->hint(), " ")
           + string("{\n") + make_padding(indent, " ") + ast->getCond()->toString(this, indent + 1) + "}";
  }
}

std::string
StyioAnalyzer::toString(AnonyFuncAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ " + " }";
}

std::string
StyioAnalyzer::toString(FuncAST* ast, int indent) {
  string suffix = "";

  if (ast->isFinal) {
    suffix = " (Final) ";
  }
  else {
    suffix = " (Flex) ";
  }

  string output = reprNodeType(ast->hint(), suffix) + "{" + "\n";

  if (ast->getId() != nullptr) {
    output += make_padding(indent, " ") + "Name: " + ast->getId()->toString(this, indent + 1) + "\n";
  }

  if (ast->getRetType() != nullptr) {
    output += make_padding(indent, " ") + "Type: " + ast->getRetType()->toString(this, indent + 1) + "\n";
  }

  output += make_padding(indent, " ") + ast->getForward()->toString(this, indent + 1) + "}";
  return output;
}

std::string
StyioAnalyzer::toString(IterAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{" + "\n"
         + make_padding(indent, " ") + ast->getIterable()->toString(this, indent + 1) + "\n"
         + make_padding(indent, " ") + ast->getForward()->toString(this, indent + 1)
         + "}";
}

std::string
StyioAnalyzer::toString(LoopAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{" + "\n"
         + make_padding(indent, " ") + ast->getForward()->toString(this, indent + 1)
         + "}";
}

std::string
StyioAnalyzer::toString(CasesAST* ast, int indent) {
  string outstr = "";

  auto Cases = ast->getCases();
  for (int i = 0; i < Cases.size(); i++) {
    outstr += make_padding(indent, " ") + "Left  : " + std::get<0>(Cases[i])->toString(this, indent + 1) + "\n";
    outstr += make_padding(indent, " ") + "Right : " + std::get<1>(Cases[i])->toString(this, indent + 1) + "\n";
  }

  return reprNodeType(ast->hint(), " ") + "{\n"
         + outstr
         + make_padding(indent, " ") + "Default: " + ast->getLastExpr()->toString(this, indent + 1)
         + "}";
}

std::string
StyioAnalyzer::toString(MatchCasesAST* ast, int indent) {
  return reprNodeType(ast->hint(), " ") + "{ " + " }";
}

std::string
StyioAnalyzer::toString(BlockAST* ast, int indent) {
  string outstr;

  auto Stmts = ast->getStmts();
  if (Stmts.empty())
    return reprNodeType(ast->hint(), " ") + "{ " + " }";

  for (int i = 0; i < Stmts.size(); i++) {
    outstr += make_padding(indent, " ") + Stmts[i]->toString(this, indent + 1);
    if (i < (Stmts.size() - 1)) {
      outstr += "\n";
    }
  }

  return reprNodeType(ast->hint(), " ") + "{\n" + outstr + "}";
}

std::string
StyioAnalyzer::toString(MainBlockAST* ast, int indent) {
  string outstr;

  auto Stmts = ast->getStmts();
  if (Stmts.empty())
    return reprNodeType(ast->hint(), " ") + "{ " + " }";

  for (int i = 0; i < Stmts.size(); i++) {
    outstr += make_padding(indent, " ") + Stmts[i]->toString(this, indent + 1);
    if (i < (Stmts.size() - 1)) {
      outstr += "\n";
    }
  }

  return reprNodeType(ast->hint(), " ") + "{\n" + outstr + "}";
}
