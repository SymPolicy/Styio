

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
  return ast->getAsStr();
}

std::string
StyioRepr::toString(TypeAST* ast, int indent) {
  return reprASTType(ast->getNodeType()) + " { " + ast->type.name + " }";
}

std::string
StyioRepr::toString(TypeTupleAST* ast, int indent) {
  std::string type_str = "\n";
  for (size_t i = 0; i < ast->type_list.size(); i++) {
    type_str += make_padding(indent + 1) + ast->type_list.at(i)->toString(this, indent + 2);
    if (i < ast->type_list.size() - 1) {
      type_str += "\n";
    }
  }

  return reprASTType(ast->getNodeType()) + " {"
         + type_str
         + "}";
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
  return reprASTType(ast->getNodeType(), " ") + "{ " + ast->getValue() + " }";
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
StyioRepr::toString(ParamAST* ast, int indent) {
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
         + "\n" + make_padding(indent) + "var : " + ast->getVar()->toString(this)
         + "\n" + make_padding(indent) + "val = " + ast->getValue()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(FinalBindAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{")
         + "\n" + make_padding(indent) + "var : " + ast->getVar()->toString(this, indent + 1)
         + "\n" + make_padding(indent) + "val = " + ast->getValue()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(ParallelAssignAST* ast, int indent) {
  std::string out = reprASTType(ast->getNodeType(), " ") + "{\n";
  out += make_padding(indent) + "lhs:\n";
  for (auto* expr : ast->getLHS()) {
    out += make_padding(indent + 1) + expr->toString(this, indent + 2) + "\n";
  }
  out += make_padding(indent) + "rhs:\n";
  for (size_t i = 0; i < ast->getRHS().size(); ++i) {
    out += make_padding(indent + 1) + ast->getRHS()[i]->toString(this, indent + 2);
    if (i + 1 < ast->getRHS().size()) {
      out += "\n";
    }
  }
  out += "}";
  return out;
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
  for (size_t i = 0; i < ast->args.size(); i++) {
    argstr += make_padding(indent) + ast->args.at(i)->toString(this, indent + 1);
    if (i != ast->args.size() - 1) {
      argstr += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " {\n") + argstr + "}";
}

std::string
StyioRepr::toString(TupleAST* ast, int indent) {
  string elem_str;

  auto elems = ast->elements;
  for (int i = 0; i < elems.size(); i++) {
    elem_str += make_padding(indent) + elems[i]->toString(this, indent + 1);
    if (i < (elems.size() - 1)) {
      elem_str += "\n";
    }
  }

  return reprASTType(ast->getNodeType()) + " : "
         + ast->getDataType().name
         + " (\n" + elem_str + ")";
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
StyioRepr::toString(ExtractorAST* ast, int indent) {
  return reprASTType(ast->getNodeType()) + " {\n"
         + make_padding(indent) + ast->theTuple->toString(this, indent + 1) + "\n"
         + make_padding(indent) + ast->theOpOnIt->toString(this, indent + 1)
         + "}";
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
    return reprASTType(ast->getNodeType()) + " [ ]";
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
StyioRepr::toString(DictAST* ast, int indent) {
  if (ast->getEntries().empty()) {
    return reprASTType(ast->getNodeType()) + " { }";
  }

  std::string entry_str;
  auto const& entries = ast->getEntries();
  for (size_t i = 0; i < entries.size(); ++i) {
    entry_str += make_padding(indent) + "Key: "
      + entries[i].key->toString(this, indent + 1)
      + "\n"
      + make_padding(indent) + "Val: "
      + entries[i].value->toString(this, indent + 1);
    if (i + 1 < entries.size()) {
      entry_str += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " : ")
    + ast->getDataType().name + " {\n" + entry_str + "\n}";
}

std::string
StyioRepr::toString(SizeOfAST* ast, int indent) {
  return reprASTType(ast->getNodeType()) + " { " + ast->getValue()->toString(this, indent + 1) + " }";
}

std::string
StyioRepr::toString(ListOpAST* ast, int indent) {
  auto OpType = ast->getOp();
  switch (OpType) {
    case StyioNodeType::Access:
      return reprASTType(ast->getNodeType(), " ") + "{"
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Key: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;

    case StyioNodeType::Access_By_Index:
      return reprASTType(ast->getNodeType(), " ")
             + "{\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeType::Access_By_Name:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Name : " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;

    case StyioNodeType::Get_Index_By_Value:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeType::Get_Indices_By_Many_Values:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeType::Append_Value:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Value: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeType::Insert_Item_By_Index:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Value: " + ast->getSlot2()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeType::Remove_Item_By_Index:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeType::Remove_Item_By_Value:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Value: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeType::Remove_Items_By_Many_Indices:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Index: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeType::Remove_Items_By_Many_Values:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "\n" + make_padding(indent) + "Value: " + ast->getSlot1()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeType::Get_Reversed:
      return reprASTType(ast->getNodeType(), " ")
             + "\n" + make_padding(indent) + ast->getList()->toString(this, indent + 1)
             + "}";
      break;
    case StyioNodeType::Get_Index_By_Item_From_Right:
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

std::string
StyioRepr::toString(UndefinedLitAST* ast, int indent) {
  (void)ast;
  (void)indent;
  return "@";
}

std::string
StyioRepr::toString(WaveMergeAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + " {\n"
         + make_padding(indent) + "cond: " + ast->getCond()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "true: " + ast->getTrueVal()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "false: " + ast->getFalseVal()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(WaveDispatchAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + " {\n"
         + make_padding(indent) + "cond: " + ast->getCond()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "true: " + ast->getTrueArm()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "false: " + ast->getFalseArm()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(FallbackAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + " {\n"
         + make_padding(indent) + "primary: " + ast->getPrimary()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "alt: " + ast->getAlternate()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(GuardSelectorAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + " {\n"
         + make_padding(indent) + "base: " + ast->getBase()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "cond: " + ast->getCond()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(EqProbeAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + " {\n"
         + make_padding(indent) + "base: " + ast->getBase()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "probe: " + ast->getProbeValue()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(FileResourceAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + " {\n"
         + make_padding(indent) + "auto_detect: " + (ast->isAutoDetect() ? "true" : "false") + "\n"
         + make_padding(indent) + "path: " + ast->getPath()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(StdStreamAST* ast, int indent) {
  const char* name = "unknown";
  switch (ast->getStreamKind()) {
    case StdStreamKind::Stdin:  name = "stdin";  break;
    case StdStreamKind::Stdout: name = "stdout"; break;
    case StdStreamKind::Stderr: name = "stderr"; break;
  }
  return std::string("styio.ast.std_stream { @") + name + " }";
}

std::string
StyioRepr::toString(HandleAcquireAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + " {\n"
         + make_padding(indent) + "var: " + ast->getVar()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "mode: "
         + (ast->isFlexBind() ? std::string("flex") : std::string("final")) + "\n"
         + make_padding(indent) + "resource: " + ast->getResource()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(ResourceWriteAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + " {\n"
         + make_padding(indent) + "data: " + ast->getData()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "resource: " + ast->getResource()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(ResourceRedirectAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ")
         + " {\n"
         + make_padding(indent) + "data: " + ast->getData()->toString(this, indent + 1) + "\n"
         + make_padding(indent) + "resource: " + ast->getResource()->toString(this, indent + 1)
         + "}";
}

std::string
StyioRepr::toString(StateDeclAST* ast, int indent) {
  (void)indent;
  return reprASTType(ast->getNodeType(), " ") + " { }";
}

std::string
StyioRepr::toString(StateRefAST* ast, int indent) {
  (void)indent;
  return reprASTType(ast->getNodeType(), " ") + " $" + ast->getNameStr();
}

std::string
StyioRepr::toString(HistoryProbeAST* ast, int indent) {
  (void)indent;
  return reprASTType(ast->getNodeType(), " ") + " { }";
}

std::string
StyioRepr::toString(SeriesIntrinsicAST* ast, int indent) {
  (void)indent;
  return reprASTType(ast->getNodeType(), " ") + " { }";
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
  string var_str;

  auto res_list = ast->res_list;
  for (int i = 0; i < res_list.size(); i++) {
    var_str += make_padding(indent) + res_list[i].first->toString(this, indent + 1) + "\n";
    var_str += make_padding(indent) + "type: { " + res_list[i].second + " }";
    if (i < (res_list.size() - 1)) {
      var_str += "\n";
    }
  }

  return reprASTType(ast->getNodeType(), " ") + "{\n" + var_str + "}";
}

std::string
StyioRepr::toString(ResPathAST* ast, int indent) {
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
StyioRepr::toString(ContinueAST* ast, int indent) {
  (void)ast;
  (void)indent;
  return reprASTType(StyioNodeType::Continue, " ") + string("{ }");
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
StyioRepr::toString(FuncCallAST* ast, int indent) {
  std::string out_str;

  out_str += reprASTType(ast->getNodeType(), " {\n");

  if (ast->func_callee) {
    out_str += make_padding(indent) + ast->func_callee->toString(this, indent + 1) + "\n";
  }

  out_str += make_padding(indent) + ast->getFuncName()->toString(this, indent + 1) + " {\n";

  string args_str;
  auto call_args = ast->getArgList();
  for (int i = 0; i < call_args.size(); i++) {
    args_str += make_padding(indent + 1) + call_args[i]->toString(this, indent + 2);
    if (i < (call_args.size() - 1)) {
      args_str += "\n";
    }
  }
  out_str += args_str + "}}";

  return out_str;
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

  if (not ast->exprs.empty()) {
    for (int i = 0; i < ast->exprs.size(); i++) {
      outstr += make_padding(indent) + ast->exprs[i]->toString(this, indent + 1);
      if (i < (ast->exprs.size() - 1)) {
        outstr += "\n";
      }
    }
  }

  return reprASTType(ast->getNodeType(), " ") + string("{\n") + outstr + "}";
}

std::string
StyioRepr::toString(ForwardAST* ast, int indent) {
  // switch (ast->getNodeType()) {
  //   case StyioASTType::Forward: {
  //     return reprASTType(ast->getNodeType(), " ") + "{\n"
  //            + make_padding(indent) + "Next: " + ast->getThen()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::If_Equal_To_Forward: {
  //     return reprASTType(ast->getNodeType(), " ") + "{\n"
  //            + make_padding(indent) + ast->getCheckEq()->toString(this, indent + 1) + "\n"
  //            + make_padding(indent) + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::If_Is_In_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getCheckIsin()->toString(this, indent + 1) + "\n"
  //            + make_padding(indent) + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::Cases_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + "Cases: " + ast->getThen()->toString(this, indent + 1) + "\n"
  //            + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::If_True_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::If_False_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;

  //   case StyioASTType::If_Both_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::Fill_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
  //            + make_padding(indent) + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::Fill_If_Equal_To_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
  //            + make_padding(indent) + ast->getCheckEq()->toString(this, indent + 1) + "\n"
  //            + make_padding(indent) + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::Fill_If_Is_in_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
  //            + make_padding(indent) + ast->getCheckIsin()->toString(this, indent + 1) + "\n"
  //            + make_padding(indent) + "Run: " + ast->getThen()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::Fill_Cases_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1)
  //            + "\n" + make_padding(indent) + "Cases: " + ast->getThen()->toString(this, indent + 1)
  //            + "\n" + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::Fill_If_True_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
  //            + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   case StyioASTType::Fill_If_False_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
  //            + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;

  //   case StyioASTType::Fill_If_Both_Forward: {
  //     return reprASTType(ast->getNodeType(), " ")
  //            + "{\n" + make_padding(indent) + ast->getVarTuple()->toString(this, indent + 1) + "\n"
  //            + make_padding(indent) + ast->getCondFlow()->toString(this, indent + 1) + "}";
  //   }
  //   // You should NOT reach this line!
  //   break;
  //   default:
  //     break;
  // }
  return reprASTType(ast->getNodeType(), " ") + string("{ Undefined }");
}

std::string
StyioRepr::toString(BackwardAST* ast, int indent) {
  return reprASTType(ast->getNodeType()) + string(" { ") + " }";
}

std::string
StyioRepr::toString(CheckEqualAST* ast, int indent) {
  std::string outstr = reprASTType(ast->getNodeType()) + " {\n";

  for (size_t i = 0; i < ast->right_values.size(); i++) {
    outstr += make_padding(indent) + ast->right_values[i]->toString(this, indent + 1);
  }

  outstr += "}";

  return outstr;
}

std::string
StyioRepr::toString(CheckIsinAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + string("{\n")
         + make_padding(indent) + ast->getIterable()->toString(this, indent + 1) + "}";
}

std::string
StyioRepr::toString(HashTagNameAST* ast, int indent) {
  std::string outstr = reprASTType(ast->getNodeType()) + string(" { ");

  for (auto i : ast->words) {
    outstr += i + " ";
  }

  outstr += "}";

  return outstr;
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

  if (WhatFlow == StyioNodeType::CondFlow_True || WhatFlow == StyioNodeType::CondFlow_False) {
    return reprASTType(ast->getNodeType(), " ")
           + string("{\n")
           + make_padding(indent) + ast->getCond()->toString(this, indent + 1) + "\n"
           + make_padding(indent) + "Then: " + ast->getThen()->toString(this, indent + 1) + "}";
  }
  else if (WhatFlow == StyioNodeType::CondFlow_Both) {
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
StyioRepr::toString(FunctionAST* ast, int indent) {
  string suffix = "";

  if (ast->is_unique) {
    suffix = ".unique";
  }

  string output = reprASTType(ast->getNodeType(), suffix) + "{" + "\n";

  if (ast->func_name) {
    output += make_padding(indent) + "func_name: " + ast->func_name->toString(this, indent + 1) + "\n";
  }

  if (not ast->params.empty()) {
    std::string param_str = "\n";
    for (size_t i = 0; i < ast->params.size(); i++) {
      param_str += make_padding(indent + 1) + ast->params.at(i)->toString(this, indent + 2);
      param_str += "\n";
    }
    output += make_padding(indent) + "params: " + param_str;
  }

  if (not ast->ret_type.valueless_by_exception()) {
    std::string ret_type_str;

    if (std::holds_alternative<TypeAST*>(ast->ret_type) && std::get<TypeAST*>(ast->ret_type)) {
      ret_type_str = std::get<TypeAST*>(ast->ret_type)->toString(this, indent + 1);
    }
    else if (std::holds_alternative<TypeTupleAST*>(ast->ret_type) && std::get<TypeTupleAST*>(ast->ret_type)) {
      ret_type_str = std::get<TypeTupleAST*>(ast->ret_type)->toString(this, indent + 1);
    }

    output += make_padding(indent) + "ret_type: " + ret_type_str + "\n";
  }

  output += make_padding(indent) + "func_body:\n"
            + make_padding(indent + 1) + ast->func_body->toString(this, indent + 2) + "}";
  return output;
}

std::string
StyioRepr::toString(SimpleFuncAST* ast, int indent) {
  std::string suffix;
  if (ast->is_unique) {
    suffix = ".unique";
  }

  string output = reprASTType(ast->getNodeType(), suffix) + " {" + "\n";

  if (ast->func_name) {
    output += make_padding(indent) + "func_name: " + ast->func_name->toString(this, indent + 1) + "\n";
  }

  if (not ast->params.empty()) {
    std::string param_str = "\n";
    for (size_t i = 0; i < ast->params.size(); i++) {
      param_str += make_padding(indent + 1) + ast->params.at(i)->toString(this, indent + 2);
      param_str += "\n";
    }
    output += make_padding(indent) + "params: " + param_str;
  }

  if (not ast->ret_type.valueless_by_exception()) {
    std::string ret_type_str;

    if (std::holds_alternative<TypeAST*>(ast->ret_type)) {
      if (std::get<TypeAST*>(ast->ret_type)) {
        ret_type_str = std::get<TypeAST*>(ast->ret_type)->toString(this, indent + 1);
      }
    }
    else if (std::holds_alternative<TypeTupleAST*>(ast->ret_type)) {
      if (std::get<TypeTupleAST*>(ast->ret_type)) {
        ret_type_str = std::get<TypeTupleAST*>(ast->ret_type)->toString(this, indent + 1);
      }
    }

    output += make_padding(indent) + "ret_type: " + ret_type_str + "\n";
  }

  output += make_padding(indent) + "ret_expr: " + ast->ret_expr->toString(this, indent + 1) + "}";
  return output;
}

std::string
StyioRepr::toString(IteratorAST* ast, int indent) {
  std::string outstr = reprASTType(ast->getNodeType()) + " {" + "\n";

  outstr += make_padding(indent) + "iterable object: " + ast->collection->toString(this, indent + 1) + "\n";

  for (size_t i = 0; i < ast->params.size(); i++) {
    outstr += make_padding(indent) + ast->params.at(i)->toString(this, indent + 1) + "\n";
  }

  if (not ast->following.empty()) {
    for (size_t i = 0; i < ast->following.size(); i++) {
      outstr += make_padding(indent) + ast->following.at(i)->toString(this, indent + 1);
      if (i < ast->following.size() - 1) {
        outstr += "\n";
      }
    }
  }

  outstr += "}";

  return outstr;
}

std::string
StyioRepr::toString(StreamZipAST* ast, int indent) {
  std::string outstr = reprASTType(ast->getNodeType()) + " {\n";
  outstr += make_padding(indent) + "a: " + ast->getCollectionA()->toString(this, indent + 1) + "\n";
  for (auto* p : ast->getParamsA()) {
    outstr += make_padding(indent) + p->toString(this, indent + 1) + "\n";
  }
  outstr += make_padding(indent) + "b: " + ast->getCollectionB()->toString(this, indent + 1) + "\n";
  for (auto* p : ast->getParamsB()) {
    outstr += make_padding(indent) + p->toString(this, indent + 1) + "\n";
  }
  if (!ast->getFollowing().empty()) {
    outstr += ast->getFollowing()[0]->toString(this, indent + 1);
  }
  outstr += "\n}";
  return outstr;
}

std::string
StyioRepr::toString(SnapshotDeclAST* ast, int indent) {
  return reprASTType(ast->getNodeType()) + " { " + ast->getVar()->getAsStr() + " <- "
         + ast->getResource()->toString(this, indent + 1) + " }";
}

std::string
StyioRepr::toString(InstantPullAST* ast, int indent) {
  return reprASTType(ast->getNodeType()) + " { "
         + ast->getResource()->toString(this, indent + 1) + " }";
}

std::string
StyioRepr::toString(TypedStdinListAST* ast, int indent) {
  return reprASTType(ast->getNodeType()) + " { "
         + ast->getListType()->toString(this, indent + 1) + " }";
}

std::string
StyioRepr::toString(IterSeqAST* ast, int indent) {
  std::string outstr = reprASTType(ast->getNodeType(), " ") + "{" + "\n";

  if (ast->collection) {
    outstr += make_padding(indent) + ast->collection->toString(this, indent + 1) + "\n";
  }

  for (size_t i = 0; i < ast->hash_tags.size(); i++) {
    outstr += make_padding(indent) + ast->hash_tags.at(i)->toString(this, indent + 1);
    if (i < ast->hash_tags.size() - 1) {
      outstr += "\n";
    }
  }

  outstr += "}";

  return outstr;
}

std::string
StyioRepr::toString(InfiniteLoopAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{\n"
         + make_padding(indent) + ast->getBody()->toString(this, indent + 1)
         + "\n}";
}

std::string
StyioRepr::toString(CasesAST* ast, int indent) {
  string outstr = reprASTType(ast->getNodeType(), " ") + "{\n";

  std::string case_str;
  auto Cases = ast->getCases();
  for (int i = 0; i < Cases.size(); i++) {
    case_str += make_padding(indent) + "(case) " + std::get<0>(Cases[i])->toString(this, indent + 1) + "\n";
    case_str += make_padding(indent) + std::get<1>(Cases[i])->toString(this, indent + 1) + "\n";
  }
  outstr += case_str;

  if (ast->case_default) {
    outstr += make_padding(indent) + "(default) " + ast->case_default->toString(this, indent + 1);
  }

  outstr += "}";

  return outstr;
}

std::string
StyioRepr::toString(MatchCasesAST* ast, int indent) {
  return reprASTType(ast->getNodeType(), " ") + "{ " + " }";
}

std::string
StyioRepr::toString(BlockAST* ast, int indent) {
  string outstr;

  auto stmts = ast->stmts;
  for (int i = 0; i < stmts.size(); i++) {
    outstr += make_padding(indent) + stmts.at(i)->toString(this, indent + 1);
    if (i < (stmts.size() - 1)) {
      outstr += "\n";
    }
  }

  auto followings = ast->followings;
  if (not followings.empty()) {
    outstr += "\n";
    for (int i = 0; i < followings.size(); i++) {
      outstr += make_padding(indent + i) + followings.at(i)->toString(this, indent + i + 1);
      if (i < (followings.size() - 1)) {
        outstr += "\n";
      }
    }
  }

  return reprASTType(ast->getNodeType(), " ") + "{\n" + outstr + "}";
}

std::string
StyioRepr::toString(MainBlockAST* ast, int indent) {
  string outstr;

  auto Stmts = ast->getStmts();
  if (Stmts.empty())
    return reprASTType(ast->getNodeType(), " { }");

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
  return std::string("styio.ir.id { ") + node->as_str() + " }";
}

std::string
StyioRepr::toString(SGType* node, int indent) {
  return std::string("styio.ir.type { ")
         + reprDataTypeOption(node->data_type.option) + ", "
         + node->data_type.name + ", "
         + std::to_string(node->data_type.num_of_bit)
         + " }";
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
  std::string argstr;
  for (size_t i = 0; i < node->elements.size(); i++) {
    argstr += make_padding(indent) + node->elements.at(i)->toString(this, indent + 1);
    if (i != node->elements.size() - 1) {
      argstr += "\n";
    }
  }
  return std::string("styio.ir.struct {\n") + argstr + "}";
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
  std::string output = std::string("styio.ir.var {\n");
  output += make_padding(indent) + node->var_name->toString(this, indent + 1) + "\n";

  if (node->val_init) {
    output += make_padding(indent) + node->var_type->toString(this, indent + 1) + "\n";
    output += make_padding(indent) + node->val_init->toString(this, indent + 1);
  }
  else {
    output += make_padding(indent) + node->var_type->toString(this, indent + 1);
  }

  output += "}";

  return output;
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
StyioRepr::toString(SGDynLoad* node, int indent) {
  (void)indent;
  return std::string("styio.ir.dyn_load { ") + node->var_name + " }";
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
  if (node->stmts.empty())
    return "styio.ir.main { }";

  std::string stmtstr;
  for (size_t i = 0; i < node->stmts.size(); i++) {
    stmtstr += make_padding(indent) + node->stmts.at(i)->toString(this, indent + 1) + "\n";
  }

  return std::string("styio.ir.main {\n") + stmtstr + "}";
}

std::string
StyioRepr::toString(SGLoop* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.loop { }";
}

std::string
StyioRepr::toString(SGForEach* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.foreach { }";
}

std::string
StyioRepr::toString(SGRangeFor* node, int indent) {
  std::string s = "styio.ir.range_for { ";
  s += "var=" + node->var;
  s += ", start=" + (node->start ? node->start->toString(this, indent) : std::string("null"));
  s += ", end=" + (node->end ? node->end->toString(this, indent) : std::string("null"));
  s += ", step=" + (node->step ? node->step->toString(this, indent) : std::string("null"));
  s += " }";
  return s;
}

std::string
StyioRepr::toString(SGIf* node, int indent) {
  std::string s = "styio.ir.if { cond="
    + (node->cond ? node->cond->toString(this, indent) : std::string("null"));
  s += ", then=" + (node->then_block ? node->then_block->toString(this, indent) : std::string("null"));
  if (node->else_block) {
    s += ", else=" + node->else_block->toString(this, indent);
  }
  s += " }";
  return s;
}

std::string
StyioRepr::toString(SGListLiteral* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.listlit { }";
}

std::string
StyioRepr::toString(SGDictLiteral* node, int indent) {
  (void)indent;
  return std::string("styio.ir.dict_literal { entries=") + std::to_string(node->entries.size())
    + ", value=" + node->value_type + " }";
}

std::string
StyioRepr::toString(SGStateSnapLoad* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.state.snap { }";
}

std::string
StyioRepr::toString(SGStateHistLoad* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.state.hist { }";
}

std::string
StyioRepr::toString(SGSeriesAvgStep* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.series.avg { }";
}

std::string
StyioRepr::toString(SGSeriesMaxStep* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.series.max { }";
}

std::string
StyioRepr::toString(SGMatch* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.match { }";
}

std::string
StyioRepr::toString(SGBreak* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.break { }";
}

std::string
StyioRepr::toString(SGContinue* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.continue { }";
}

std::string
StyioRepr::toString(SGUndef* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.undef { }";
}

std::string
StyioRepr::toString(SGFallback* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.fallback { }";
}

std::string
StyioRepr::toString(SGWaveMerge* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.wave_merge { }";
}

std::string
StyioRepr::toString(SGWaveDispatch* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.wave_dispatch { }";
}

std::string
StyioRepr::toString(SGGuardSelect* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.guard { }";
}

std::string
StyioRepr::toString(SGEqProbe* node, int indent) {
  (void)node;
  (void)indent;
  return "styio.ir.eq_probe { }";
}

std::string
StyioRepr::toString(SGHandleAcquire* node, int indent) {
  std::string p = node->path_expr ? node->path_expr->toString(this, indent) : std::string("null");
  return std::string("styio.ir.handle_acquire { ") + node->var_name + ", auto="
         + (node->is_auto ? "1" : "0") + ", path=" + p + " }";
}

std::string
StyioRepr::toString(SGFileLineIter* node, int indent) {
  std::string s = "styio.ir.file_line_iter { ";
  if (node->from_path) {
    s += "path=" + (node->path_expr ? node->path_expr->toString(this, indent) : std::string("null"));
  }
  else {
    s += "handle=" + node->handle_var;
  }
  s += ", line=" + node->line_var;
  s += ", body=" + (node->body ? node->body->toString(this, indent) : std::string("{}"));
  s += " }";
  return s;
}

std::string
StyioRepr::toString(SGStreamZip* node, int indent) {
  (void)indent;
  return std::string("styio.ir.stream_zip { ") + node->var_a + " & " + node->var_b + " }";
}

std::string
StyioRepr::toString(SGSnapshotDecl* node, int indent) {
  (void)indent;
  std::string p = node->path_expr ? node->path_expr->toString(this, indent) : std::string("null");
  return std::string("styio.ir.snapshot_decl { ") + node->var_name + " path=" + p + " }";
}

std::string
StyioRepr::toString(SGSnapshotShadowLoad* node, int indent) {
  (void)indent;
  return std::string("styio.ir.snapshot_load { ") + node->var_name + " }";
}

std::string
StyioRepr::toString(SGInstantPull* node, int indent) {
  std::string p = node->path_expr ? node->path_expr->toString(this, indent) : std::string("null");
  return std::string("styio.ir.instant_pull { path=") + p + " }";
}

std::string
StyioRepr::toString(SGListReadStdin* node, int indent) {
  (void)indent;
  return std::string("styio.ir.list_read_stdin { elem=") + node->elem_type + " }";
}

std::string
StyioRepr::toString(SGListClone* node, int indent) {
  return std::string("styio.ir.list_clone { src=")
    + (node->source ? node->source->toString(this, indent) : std::string("null")) + " }";
}

std::string
StyioRepr::toString(SGListLen* node, int indent) {
  return std::string("styio.ir.list_len { list=")
    + (node->list ? node->list->toString(this, indent) : std::string("null")) + " }";
}

std::string
StyioRepr::toString(SGListGet* node, int indent) {
  return std::string("styio.ir.list_get { list=")
    + (node->list ? node->list->toString(this, indent) : std::string("null"))
    + ", index=" + (node->index ? node->index->toString(this, indent) : std::string("null"))
    + ", elem=" + node->elem_type + " }";
}

std::string
StyioRepr::toString(SGListSet* node, int indent) {
  return std::string("styio.ir.list_set { list=")
    + (node->list ? node->list->toString(this, indent) : std::string("null"))
    + ", index=" + (node->index ? node->index->toString(this, indent) : std::string("null"))
    + ", value=" + (node->value ? node->value->toString(this, indent) : std::string("null"))
    + ", type=" + node->elem_type + " }";
}

std::string
StyioRepr::toString(SGListToString* node, int indent) {
  return std::string("styio.ir.list_to_string { list=")
    + (node->list ? node->list->toString(this, indent) : std::string("null")) + " }";
}

std::string
StyioRepr::toString(SGDictClone* node, int indent) {
  return std::string("styio.ir.dict_clone { src=")
    + (node->source ? node->source->toString(this, indent) : std::string("null")) + " }";
}

std::string
StyioRepr::toString(SGDictLen* node, int indent) {
  return std::string("styio.ir.dict_len { dict=")
    + (node->dict ? node->dict->toString(this, indent) : std::string("null")) + " }";
}

std::string
StyioRepr::toString(SGDictGet* node, int indent) {
  return std::string("styio.ir.dict_get { dict=")
    + (node->dict ? node->dict->toString(this, indent) : std::string("null"))
    + ", key=" + (node->key ? node->key->toString(this, indent) : std::string("null"))
    + ", value=" + node->value_type + " }";
}

std::string
StyioRepr::toString(SGDictSet* node, int indent) {
  return std::string("styio.ir.dict_set { dict=")
    + (node->dict ? node->dict->toString(this, indent) : std::string("null"))
    + ", key=" + (node->key ? node->key->toString(this, indent) : std::string("null"))
    + ", value=" + (node->value ? node->value->toString(this, indent) : std::string("null"))
    + ", type=" + node->value_type + " }";
}

std::string
StyioRepr::toString(SGDictKeys* node, int indent) {
  return std::string("styio.ir.dict_keys { dict=")
    + (node->dict ? node->dict->toString(this, indent) : std::string("null")) + " }";
}

std::string
StyioRepr::toString(SGDictValues* node, int indent) {
  return std::string("styio.ir.dict_values { dict=")
    + (node->dict ? node->dict->toString(this, indent) : std::string("null"))
    + ", value=" + node->value_type + " }";
}

std::string
StyioRepr::toString(SGDictToString* node, int indent) {
  return std::string("styio.ir.dict_to_string { dict=")
    + (node->dict ? node->dict->toString(this, indent) : std::string("null")) + " }";
}

std::string
StyioRepr::toString(SGResourceWriteToFile* node, int indent) {
  std::string d = node->data_expr ? node->data_expr->toString(this, indent) : std::string("null");
  std::string p = node->path_expr ? node->path_expr->toString(this, indent) : std::string("null");
  return std::string("styio.ir.resource_write { data=") + d + ", path=" + p
         + ", auto_path=" + (node->is_auto_path ? "1" : "0")
         + ", promote=" + (node->promote_data_to_cstr ? "1" : "0")
         + ", nl=" + (node->append_newline ? "1" : "0") + " }";
}

std::string
StyioRepr::toString(SIOStdStreamWrite* node, int indent) {
  if (node->stream == SIOStdStreamWrite::Stream::Stdout) {
    return std::string("styio.ir.print { ") + " }";
  }
  std::string target = (node->stream == SIOStdStreamWrite::Stream::Stderr) ? "Stderr" : "Stdout";
  std::string parts;
  for (size_t i = 0; i < node->exprs.size(); ++i) {
    if (i > 0) parts += ", ";
    parts += node->exprs[i] ? node->exprs[i]->toString(this, indent) : std::string("null");
  }
  return std::string("styio.ir.std_stream_write { target=") + target + ", exprs=[" + parts + "] }";
}

std::string
StyioRepr::toString(SIOStdStreamLineIter* node, int indent) {
  std::string b = node->body ? node->body->toString(this, indent) : std::string("null");
  return std::string("styio.ir.stdin_line_iter { var=") + node->line_var + ", body=" + b + " }";
}

std::string
StyioRepr::toString(SIOStdStreamPull* node, int indent) {
  return std::string("styio.ir.stdin_pull { }");
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
