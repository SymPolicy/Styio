

// [C++ STL]
#include <iostream>
#include <string>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioIR/DFIR/DFIR.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioIR/IOIR/IOIR.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioUtil/Util.hpp"

/*                                                                */
/*    ___| __ __| \ \   / _ _|   _ \          \      ___| __ __|  */
/*  \___ \    |    \   /    |   |   |        _ \   \___ \    |    */
/*        |   |       |     |   |   |       ___ \        |   |    */
/*  _____/   _|      _|   ___| \___/      _/    _\ _____/   _|    */
/*                                                                */

std::string
StyioRepr::toString(CommentAST* ast, int indent) {
  return string("Comment { ") + ast->getText() + " }";
}

std::string
StyioRepr::toString(NoneAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ }";
}

std::string
StyioRepr::toString(EmptyAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ }";
}

std::string
StyioRepr::toString(NameAST* ast, int indent) {
  return ast->getNameAsStr();
}

std::string
StyioRepr::toString(DTypeAST* ast, int indent) {
  return reprASTType(ast->getNodeType()) + " { " + ast->data_type.name + " }";
}

std::string
StyioRepr::toString(BoolAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{ }");
}

std::string
StyioRepr::toString(IntAST* ast, int indent) {
  return "{ " + ast->getValue() + " : " + ast->getDataType().name + " }";
}

std::string
StyioRepr::toString(FloatAST* ast, int indent) {
  return "{ " + ast->getValue() + " : " + ast->getDataType().name + " }";
}

std::string
StyioRepr::toString(CharAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ \'" + ast->getValue() + "\' }";
}

std::string
StyioRepr::toString(StringAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ \"" + ast->getValue() + "\" }";
}

std::string
StyioRepr::toString(TypeConvertAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ " + " }";
}

std::string
StyioRepr::toString(VarAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + string("{ ") + ast->getNameAsStr() + " : " + ast->getDType()->getTypeName() + " }";
}

std::string
StyioRepr::toString(ArgAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + string("{ ") + ast->getName() + " : " + ast->getDType()->getTypeName() + " }";
}

std::string
StyioRepr::toString(OptArgAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + "{ " + " }";
}

std::string
StyioRepr::toString(OptKwArgAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + "{ " + " }";
}

std::string
StyioRepr::toString(FlexBindAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{")
         + "\n" + make_padding(indent) + ast->getVar()->toString(this)
         + "\n" + make_padding(indent) + ast->getValue()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(FinalBindAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{")
         + "\n" + make_padding(indent) + "Var= " + ast->getVarName()->toString(this, indent + 1)
         + "\n" + make_padding(indent) + "Val= " + ast->getValue()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(InfiniteAST* ast, int indent) {
  switch (ast->getType()) {
    case InfiniteType::Original: {
      return reprASTType(ast->getNodeType(), " ") + string("{ }");
    } break;  // You should NOT reach this line!

    case InfiniteType::Incremental: {
      return reprASTType(ast->getNodeType(), " ") + string("{")
             + "\n" + "|" + string(2 * indent, '-') + "| Start: " + ast->getStart()->toString(this, indent + 1)
             + "\n" + "|" + string(2 * indent, '-') + "| Increment: " + ast->getIncEl()->toString(this, indent + 1)
             + "}";

    } break;  // You should NOT reach this line!

    default:
      break;
  }

  return reprASTType(ast->getNodeType(), " ") + string("{ Undefined! }");
}

std::string
StyioRepr::toString(StructAST* ast, int indent) {
  std::string argstr;
  for (size_t i = 0; i < ast->attrs.size(); i++) {
    argstr += make_padding(indent) + ast->attrs.at(i)->toString(this, indent + 1);
  }

  return reprASTType(ast->getNodeType(), " {\n") + argstr + "}";
}

std::string
StyioRepr::toString(TupleAST* ast, int indent) {
  string ElemStr;

  auto Elems = ast->getElements();
  for (int i = 0; i < Elems.size(); i++) {
    ElemStr += make_padding(indent) + Elems[i]->toString(this, indent + 1);
    if (i < (Elems.size() - 1)) {
      ElemStr += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " : ")
         + ast->getDataType().name
         + " (\n" + ElemStr + ")";
}

std::string
StyioRepr::toString(VarTupleAST* ast, int indent) {
  auto Vars = ast->getParams();
  if (Vars.empty()) {
    return reprASTType(ast->getNodeType(), " ") + "[ ]";
  }
  else {
    string varStr;

    for (int i = 0; i < Vars.size(); i++) {
      varStr += make_padding(indent) + Vars[i]->toString(this, indent + 1);
      if (i < (Vars.size() - 1)) {
        varStr += "\n";
      }
    }

    return reprASTType(ast->getNodeType(), " ") + "[\n" + varStr + "]";
  }
}

std::string
StyioRepr::toString(RangeAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{\n"
         + make_padding(indent) + "Start : " + ast->getStart()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "End   : " + ast->getEnd()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "Step  : " + ast->getStep()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(SetAST* ast, int indent) {
  string ElemStr;

  auto Elems = ast->getElements();
  for (int i = 0; i < Elems.size(); i++) {
    ElemStr += make_padding(indent) + Elems[i]->toString(this, indent + 1);
    if (i != (Elems.size() - 1)) {
      ElemStr += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " ") + "{\n" + ElemStr + "}";
}

std::string
StyioRepr::toString(ListAST* ast, int indent) {
  if (ast->getElements().empty()) {
    return reprASTType(ast->getNodeType(), " ") + "[ ]";
  }

  string ElemStr;

  auto Elems = ast->getElements();
  for (int i = 0; i < Elems.size(); i++) {
    ElemStr += make_padding(indent) + Elems[i]->toString(this, indent + 1);
    if (i != (Elems.size() - 1)) {
      ElemStr += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " : ") + ast->getDataType().name + " [\n" + ElemStr + "]";
}

std::string
StyioRepr::toString(SizeOfAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + "{ " + ast->getValue()->toString(this, indent + 1) + " }";
}

std::string
StyioRepr::toString(ListOpAST* ast, int indent) {
  auto OpType = ast->getOp();
  switch (OpType) {
    case StyioASTType::Access:
      return reprASTType(ast->getNodeType(), " ") + "{"
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Key: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;

    case StyioASTType::Access_By_Index:
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioASTType::Access_By_Name:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Name : " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;

    case StyioASTType::Get_Index_By_Value:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioASTType::Get_Indices_By_Many_Values:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioASTType::Append_Value:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Value: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioASTType::Insert_Item_By_Index:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Value: " + ast->getSlot2()->toString(this, indent + 1)
             + "}";
      break;
    case StyioASTType::Remove_Item_By_Index:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioASTType::Remove_Item_By_Value:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Value: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioASTType::Remove_Items_By_Many_Indices:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioASTType::Remove_Items_By_Many_Values:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Value: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioASTType::Get_Reversed:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "}";
      break;
    case StyioASTType::Get_Index_By_Item_From_Right:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;

    default: {
      return reprASTType(ast->getNodeType(), " ") + string("{ undefined }");
    } break;
  }

  return reprASTType(ast->getNodeType(), " ") + string("{ undefined }");
}

std::string
StyioRepr::toString(BinCompAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + reprToken(ast->getSign()) + " {\n"
         + make_padding(indent) + "LHS: " + ast->getLHS()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "RHS: " + ast->getRHS()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(CondAST* ast, int indent) {
  LogicType LogicOp = ast->getSign();

  if (LogicOp == LogicType::AND || LogicOp == LogicType::OR || LogicOp == LogicType::XOR) {
    return reprASTType(ast->getNodeType(), " ") + "{\n"
           + make_padding(indent) + "Op: " + reprToken(LogicOp) + "\n"
           + make_padding(indent) + "LHS: " + ast->getLHS()->toString(this, indent + 1) + "\n"
           + make_padding(indent) + "RHS: " + ast->getRHS()->toString(this, indent + 1)
           + "}";
  }
  else if (LogicOp == LogicType::NOT) {
    return reprASTType(ast->getNodeType(), " ") + "\n"
           + make_padding(indent) + "Op: " + reprToken(LogicOp) + "\n"
           + make_padding(indent) + "Value: " + ast->getValue()->toString(this, indent + 1)
           + "}";
  }
  else if (LogicOp == LogicType::RAW) {
    return reprASTType(ast->getNodeType(), " {\n")
           + make_padding(indent) + ast->getValue()->toString(this, indent + 1) + "}";
  }
  else {
    return reprASTType(ast->getNodeType(), " ") + " { Undefined! }";
  }
}

/*
  Int -> Int => Pass
  Int -> Float => Pass

*/
std::string
StyioRepr::toString(BinOpAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), ": ") + ast->getType().name + " {" + "\n"
         + make_padding(indent) + "LHS: " + ast->getLHS()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "OP : " + reprToken(ast->getOp()) + "\n"
         + make_padding(indent) + "RHS: " + ast->getRHS()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(FmtStrAST* ast, int indent) {
  auto Fragments = ast->getFragments();
  auto Exprs = ast->getExprs();

  string elemstr = "";
  for (size_t i = 0; i < Fragments.size(); i++) {
    elemstr += make_padding(indent) + "\"" + Fragments[i] + "\"\n";
  }

  for (int i = 0; i < Exprs.size(); i++) {
    elemstr += make_padding(indent) + Exprs[i]->toString(this, indent + 1);
    if (i < (Exprs.size() - 1)) {
      elemstr += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " ") + "{\n" + elemstr + "}";
}

std::string
StyioRepr::toString(ResourceAST* ast, int indent) {
  string varStr;

  auto Resources = ast->getResList();
  for (int i = 0; i < Resources.size(); i++) {
    varStr += make_padding(indent) + Resources[i]->toString(this, indent + 1);
    if (i < (Resources.size() - 1)) {
      varStr += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " ") + "{\n" + varStr + "}";
}

std::string
StyioRepr::toString(LocalPathAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ " + ast->getPath() + " }";
}

std::string
StyioRepr::toString(RemotePathAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ " + ast->getPath() + " }";
}

std::string
StyioRepr::toString(WebUrlAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ " + ast->getPath() + " }";
}

std::string
StyioRepr::toString(DBUrlAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ " + ast->getPath() + " }";
}

std::string
StyioRepr::toString(ExtPackAST* ast, int indent) {
  string pathStr;

  auto PackPaths = ast->getPaths();
  for (int i = 0; i < PackPaths.size(); i++) {
    pathStr += make_padding(indent) + PackPaths[i] + "\n";
  };

  return reprASTType(ast->getNodeType(), " ") + "{\n" + pathStr + "\n}";
}

std::string
StyioRepr::toString(ReadFileAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{\n"
         + make_padding(indent) + "Var: " + ast->getId()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "Val: " + ast->getValue()->toString(this, indent + 1) + "}";
}

std::string
StyioRepr::toString(EOFAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{ }");
}

std::string
StyioRepr::toString(BreakAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{ }");
}

std::string
StyioRepr::toString(PassAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{ }");
}

std::string
StyioRepr::toString(ReturnAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{\n"
         + make_padding(indent) + ast->getExpr()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(CallAST* ast, int indent) {
  string args_str;

  auto call_args = ast->getArgList();
  for (int i = 0; i < call_args.size(); i++) {
    args_str += make_padding(indent + 1) + call_args[i]->toString(this, indent + 2);
    if (i < (call_args.size() - 1)) {
      args_str += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " {\n")
         + make_padding(indent) + ast->func_callee->toString(this, indent + 1) + "\n"
         + make_padding(indent) + ast->getFuncName()->toString(this, indent + 1) + " {\n"
         + args_str
         + "}}";
}

std::string
StyioRepr::toString(AttrAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " { ")
         + ast->body->toString(this) + "."
         + ast->attr->toString(this)
         + " }";
}

std::string
StyioRepr::toString(PrintAST* ast, int indent) {
  string outstr;

  auto Exprs = ast->getExprs();
  for (int i = 0; i < Exprs.size(); i++) {
    outstr += make_padding(indent) + Exprs[i]->toString(this, indent + 1);
    if (i < (Exprs.size() - 1)) {
      outstr += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " ") + string("{\n") + outstr + "}";
}

std::string
StyioRepr::toString(ForwardAST* ast, int indent) {
  switch (ast->getNodeType()) {
    case StyioASTType::Forward: {
      return reprASTType(ast->getNodeType(), " ") + "{\n"
             + make_padding(indent) + "Next: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::If_Equal_To_Forward: {
      return reprASTType(ast->getNodeType(), " ") + "{\n"
             + make_padding(indent) + ast->getCheckEq()->toString(this, indent + 1) + "\n"
             + make_padding(indent) + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::If_Is_In_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getCheckIsin()->toString(this, indent + 1) + "\n"
             + make_padding(indent) + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::Cases_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + "Cases: " + ast->getThen()->toString(this, indent + 1) + "\n"
             + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::If_True_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::If_False_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;

    case StyioASTType::If_Both_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::Fill_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent) + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::Fill_If_Equal_To_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent) + ast->getCheckEq()->toString(this, indent + 1) + "\n"
             + make_padding(indent) + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::Fill_If_Is_in_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent) + ast->getCheckIsin()->toString(this, indent + 1) + "\n"
             + make_padding(indent) + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::Fill_Cases_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Cases: " + ast->getThen()->toString(this, indent + 1)
             + "\n" + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::Fill_If_True_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    case StyioASTType::Fill_If_False_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;

    case StyioASTType::Fill_If_Both_Forward: {
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
             + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
    }
    // You should NOT reach this line!
    break;
    default:
      break;
  }
  return reprASTType(ast->getNodeType(), " ") + string("{ Undefined }");
}

std::string
StyioRepr::toString(CheckEqAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{ ")
         + ast->getValue()->toString(this, indent + 1) + " }";
}

std::string
StyioRepr::toString(CheckIsinAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{\n")
         + make_padding(indent) + ast->getIterable()->toString(this, indent + 1) + "}";
}

std::string
StyioRepr::toString(FromToAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{") + "\n"
         + make_padding(indent) + "From: " + ast->getFromExpr()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "To: " + ast->getToExpr()->toString(this, indent + 1) + "}";
}

std::string
StyioRepr::toString(CODPAST* ast, int indent) {
  std::string result;

  // result += reprNodeType(ast->getNodeType(), " {\n");
  result += "\n" + make_padding(indent) + "CODP." + ast->OpName + " {\n";

  auto exprs = ast->OpArgs;
  for (int i = 0; i < exprs.size(); i++) {
    result += make_padding(indent + 1) + exprs[i]->toString(this, indent + 2);
    if (i != exprs.size() - 1) {
      result += "\n";
    }
    else {
      result += "}";
    }
  }

  if (ast->NextOp) {
    result += ast->NextOp->toString(this, indent);
  }

  return result;
}

std::string
StyioRepr::toString(CondFlowAST* ast, int indent) {
  auto WhatFlow = ast->getNodeType();

  if (WhatFlow == StyioASTType::CondFlow_True || WhatFlow == StyioASTType::CondFlow_False) {
    return reprASTType(ast->getNodeType(), " ")
           + string("{\n")
           + make_padding(indent) + ast->getCond()->toString(this, indent + 1) + "\n"
           + make_padding(indent) + "Then: " + ast->getThen()->toString(this, indent + 1) + "}";
  }
  else if (WhatFlow == StyioASTType::CondFlow_Both) {
    return reprASTType(ast->getNodeType(), " ")
           + string("{\n") + make_padding(indent) + ast->getCond()->toString(this, indent + 1) + "\n"
           + make_padding(indent) + "Then: " + ast->getThen()->toString(this, indent + 1) + "\n"
           + make_padding(indent) + "Else: " + ast->getElse()->toString(this, indent + 1) + "}";
  }
  else {
    return reprASTType(ast->getNodeType(), " ")
           + string("{\n") + make_padding(indent) + ast->getCond()->toString(this, indent + 1) + "}";
  }
}

std::string
StyioRepr::toString(AnonyFuncAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ " + " }";
}

std::string
StyioRepr::toString(FuncAST* ast, int indent) {
  string suffix = "";

  if (ast->isFinal) {
    suffix = " (Final) ";
  }
  else {
    suffix = " (Flex) ";
  }

  string output = reprASTType(ast->getNodeType(), suffix) + "{" + "\n";

  if (ast->getId() != nullptr) {
    output += make_padding(indent) + "Name: " + ast->getId()->toString(this, indent + 1) + "\n";
  }

  if (ast->getRetType() != nullptr) {
    output += make_padding(indent) + "Type: " + ast->getRetType()->toString(this, indent + 1) + "\n";
  }

  output += make_padding(indent) + ast->getForward()->toString(this, indent + 1) + "}";
  return output;
}

std::string
StyioRepr::toString(IterAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{" + "\n"
         + make_padding(indent) + ast->getIterable()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + ast->getForward()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(LoopAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{" + "\n"
         + make_padding(indent) + ast->getForward()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(CasesAST* ast, int indent) {
  string outstr = "";

  auto Cases = ast->getCases();
  for (int i = 0; i < Cases.size(); i++) {
    outstr += make_padding(indent) + "Left  : " + std::get<0>(Cases[i])->toString(this, indent + 1) + "\n";
    outstr += make_padding(indent) + "Right : " + std::get<1>(Cases[i])->toString(this, indent + 1) + "\n";
  }

  return reprASTType(ast->getNodeType(), " ") + "{\n"
         + outstr
         + make_padding(indent) + "Default: " + ast->getLastExpr()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(MatchCasesAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ " + " }";
}

std::string
StyioRepr::toString(BlockAST* ast, int indent) {
  string outstr;

  auto Stmts = ast->getStmts();
  if (Stmts.empty())
    return reprASTType(ast->getNodeType(), " ") + "{ " + " }";

  for (int i = 0; i < Stmts.size(); i++) {
    outstr += make_padding(indent) + Stmts[i]->toString(this, indent + 1);
    if (i < (Stmts.size() - 1)) {
      outstr += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " ") + "{\n" + outstr + "}";
}

std::string
StyioRepr::toString(MainBlockAST* ast, int indent) {
  string outstr;

  auto Stmts = ast->getStmts();
  if (Stmts.empty())
    return reprASTType(ast->getNodeType(), " ") + "{ " + " }";

  for (int i = 0; i < Stmts.size(); i++) {
    outstr += make_padding(indent) + Stmts[i]->toString(this, indent + 1);
    if (i < (Stmts.size() - 1)) {
      outstr += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " ") + "{\n" + outstr + "}";
}

/*                                                     */
/*    ___| __ __| \ \   / _ _|   _ \      _ _|   _ \   */
/*  \___ \    |    \   /    |   |   |       |   |   |  */
/*        |   |       |     |   |   |       |   __ <   */
/*  _____/   _|      _|   ___| \___/      ___| _| \_\  */
/*                                                     */

std::string
StyioRepr::toString(SGResId* node, int indent) {
  return std::string("styio.ir.id { ") + node->id + " }";
}

std::string
StyioRepr::toString(SGType* node, int indent) {
  return std::string("styio.ir.type { ") + node->data_type.name + " }";
}

std::string
StyioRepr::toString(SGConstBool* node, int indent) {
  return std::string("styio.ir.bool { ") + std::to_string(node->value) + " }";
}

std::string
StyioRepr::toString(SGConstInt* node, int indent) {
  return std::string("styio.ir.int { ") + node->value + " }";
}

std::string
StyioRepr::toString(SGConstFloat* node, int indent) {
  return std::string("styio.ir.float { ") + node->value + " }";
}

std::string
StyioRepr::toString(SGConstChar* node, int indent) {
  return std::string("styio.ir.char { ") + node->value + " }";
}

std::string
StyioRepr::toString(SGConstString* node, int indent) {
  return std::string("styio.ir.string { ") + node->value + " }";
}

std::string
StyioRepr::toString(SGFormatString* node, int indent) {
  return std::string("styio.ir.fmtstr { ") + " }";
}

std::string
StyioRepr::toString(SGStruct* node, int indent) {
  return std::string("styio.ir.struct { ") + " }";
}

std::string
StyioRepr::toString(SGCast* node, int indent) {
  return std::string("styio.ir.cast { ") + " }";
}

std::string
StyioRepr::toString(SGBinOp* node, int indent) {
  return std::string("styio.ir.binop {\n")
  + make_padding(indent) + reprToken(node->operand) + "\n"
  + make_padding(indent) + node->lhs_expr->toString(this, indent + 1) + "\n"
  + make_padding(indent) + node->rhs_expr->toString(this, indent + 1)
  + "}";
}

std::string
StyioRepr::toString(SGCond* node, int indent) {
  return std::string("styio.ir.cond { ") + " }";
}

std::string
StyioRepr::toString(SGVar* node, int indent) {
  return std::string("styio.ir.var { ") + " }";
}

std::string
StyioRepr::toString(SGFlexBind* node, int indent) {
  return std::string("styio.ir.flex_bind { ") + " }";
}

std::string
StyioRepr::toString(SGFinalBind* node, int indent) {
  return std::string("styio.ir.final_bind { ") + " }";
}

std::string
StyioRepr::toString(SGFuncArg* node, int indent) {
  return std::string("styio.ir.func_arg { ") + " }";
}

std::string
StyioRepr::toString(SGFunc* node, int indent) {
  return std::string("styio.ir.func { ") + " }";
}

std::string
StyioRepr::toString(SGCall* node, int indent) {
  return std::string("styio.ir.call { ") + " }";
}

std::string
StyioRepr::toString(SGReturn* node, int indent) {
  return std::string("styio.ir.return { ") + " }";
}

// std::string
// StyioRepr::toString(SGIfElse* node, int indent) {
//   return std::string("styio.ir.if_else { ") + " }";
// }

// std::string
// StyioRepr::toString(SGForLoop* node, int indent) {
//   return std::string("styio.ir.for { ") + " }";
// }

// std::string
// StyioRepr::toString(SGWhileLoop* node, int indent) {
//   return std::string("styio.ir.while { ") + " }";
// }

std::string
StyioRepr::toString(SGBlock* node, int indent) {
  return std::string("styio.ir.block { ") + " }";
}

std::string
StyioRepr::toString(SGEntry* node, int indent) {
  return std::string("styio.ir.entry { ") + " }";
}

std::string
StyioRepr::toString(SGMainEntry* node, int indent) {
  std::string stmtstr;

  for (size_t i = 0; i < node->stmts.size(); i++) {
    stmtstr += make_padding(indent) + node->stmts.at(i)->toString(this, indent+1);
  }

  return std::string("styio.ir.main {\n") + stmtstr + "}";
}

std::string
StyioRepr::toString(SIOPath* node, int indent) {
  return std::string("styio.ir.path { ") + " }";
}

std::string
StyioRepr::toString(SIOPrint* node, int indent) {
  return std::string("styio.ir.print { ") + " }";
}

std::string
StyioRepr::toString(SIORead* node, int indent) {
  return std::string("styio.ir.read { ") + " }";
}