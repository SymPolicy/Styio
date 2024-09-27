// [C++ STL]
#include <string>

// [Styio]
#include "Token.hpp"

std::string
reprASTType(StyioASTType type, std::string extra) {
  std::string output = "styio.ast.";

  switch (type) {
    case StyioASTType::True: {
      output += std::string("true");
    } break;

    case StyioASTType::False: {
      output += std::string("false");
    } break;

    case StyioASTType::None: {
      output += std::string("none");
    } break;

    case StyioASTType::Empty: {
      output += std::string("empty");
    } break;

    case StyioASTType::Id: {
      output += std::string("name");
    } break;

    case StyioASTType::DType: {
      output += std::string("type");
    } break;

    case StyioASTType::TypeTuple: {
      output += std::string("types");
    } break;

    case StyioASTType::Variable: {
      output += std::string("var");
    } break;

    case StyioASTType::Param: {
      output += std::string("arg");
    } break;

    case StyioASTType::Integer: {
      auto name = std::string("styio.ast.int");

      output += std::string("");
    } break;

    case StyioASTType::Float: {
      output += std::string("float");
    } break;

    case StyioASTType::Char: {
      output += std::string("char");
    } break;

    case StyioASTType::String: {
      output += std::string("string");
    } break;

    case StyioASTType::NumConvert: {
      output += std::string("convert");
    } break;

    case StyioASTType::FmtStr: {
      output += std::string("fmtstr");
    } break;

    case StyioASTType::LocalPath: {
      output += std::string("path");
    } break;

    case StyioASTType::RemotePath: {
      output += std::string("addr");
    } break;

    case StyioASTType::WebUrl: {
      output += std::string("url");
    } break;

    case StyioASTType::DBUrl: {
      output += std::string("url.database");
    } break;

    case StyioASTType::ExtPack: {
      output += std::string("package");
    } break;

    case StyioASTType::Parameters: {
      output += std::string("vars");
    } break;

    case StyioASTType::Condition: {
      output += std::string("cond");
    } break;

    case StyioASTType::SizeOf: {
      output += std::string("sizeof");
    } break;

    case StyioASTType::BinOp: {
      output += std::string("binop");
    } break;

    case StyioASTType::Print: {
      output += std::string("print");
    } break;

    case StyioASTType::ReadFile: {
      output += std::string("read.file");
    } break;

    case StyioASTType::Call: {
      output += std::string("call");
    } break;

    case StyioASTType::Attribute: {
      output += std::string("attr");
    } break;

    case StyioASTType::Access: {
      output += std::string("access");
    } break;

    case StyioASTType::Access_By_Name: {
      output += std::string("access.by_name");
    } break;

    case StyioASTType::Access_By_Index: {
      output += std::string("access.by_index");
    } break;

    case StyioASTType::Get_Index_By_Value: {
      output += std::string("get_index.by_value");
    } break;

    case StyioASTType::Get_Indices_By_Many_Values: {
      output += std::string("get_indices.by_values");
    } break;

    case StyioASTType::Append_Value: {
      output += std::string("append");
    } break;

    case StyioASTType::Insert_Item_By_Index: {
      output += std::string("insert.by_index");
    } break;

    case StyioASTType::Remove_Item_By_Index: {
      output += std::string("remove.by_index");
    } break;

    case StyioASTType::Remove_Items_By_Many_Indices: {
      output += std::string("remove.by_indices");
    } break;

    case StyioASTType::Remove_Item_By_Value: {
      output += std::string("remove.by_value");
    } break;

    case StyioASTType::Remove_Items_By_Many_Values: {
      output += std::string("remove.by_values");
    } break;

    case StyioASTType::Get_Reversed: {
      output += std::string("reversed");
    } break;

    case StyioASTType::Get_Index_By_Item_From_Right: {
      output += std::string("get_index.by_item_backward");
    } break;

    case StyioASTType::Return: {
      output += std::string("return");
    } break;

    case StyioASTType::Range: {
      output += std::string("range");
    } break;

    case StyioASTType::Tuple: {
      output += std::string("tuple");
    } break;

    case StyioASTType::List: {
      output += std::string("list");
    } break;

    case StyioASTType::Set: {
      output += std::string("set");
    } break;

    case StyioASTType::Resources: {
      output += std::string("resources");
    } break;

    case StyioASTType::MutBind: {
      output += std::string("bind.flex");
    } break;

    case StyioASTType::FinalBind: {
      output += std::string("styio.ast.bind.final");
    } break;

    case StyioASTType::Block: {
      output += std::string("block");
    } break;

    case StyioASTType::Cases: {
      output += std::string("cases");
    } break;

    case StyioASTType::Func: {
      output += std::string("func");
    } break;

    case StyioASTType::SimpleFunc: {
      output += std::string("func.simple");
    } break;

    case StyioASTType::Struct: {
      output += std::string("struct");
    } break;

    case StyioASTType::Loop: {
      output += std::string("loop");
    } break;

    case StyioASTType::Iterator: {
      output += std::string("iterator");
    } break;

    case StyioASTType::CheckEq: {
      output += std::string("check.equal");
    } break;

    case StyioASTType::CheckIsin: {
      output += std::string("check.isin");
    } break;

    case StyioASTType::FromTo: {
      output += std::string("from_to");
    } break;

    case StyioASTType::TupleOperation: {
      output += std::string("tuple.op");
    } break;

    case StyioASTType::Forward: {
      output += std::string("forward.run");
    } break;

    case StyioASTType::If_Equal_To_Forward: {
      output += std::string("forward.check.equal");
    } break;

    case StyioASTType::If_Is_In_Forward: {
      output += std::string("forward.check.isin");
    } break;

    case StyioASTType::Cases_Forward: {
      output += std::string("forward.cases");
    } break;

    case StyioASTType::If_True_Forward: {
      output += std::string("forward.only_true");
    } break;

    case StyioASTType::If_False_Forward: {
      output += std::string("forward.only_false");
    } break;

    case StyioASTType::Fill_Forward: {
      output += std::string("forward.fill.run");
    } break;

    case StyioASTType::Fill_If_Equal_To_Forward: {
      output += std::string("fill.check.equal");
    } break;

    case StyioASTType::Fill_If_Is_in_Forward: {
      output += std::string("fill.check.isin");
    } break;

    case StyioASTType::Fill_Cases_Forward: {
      output += std::string("fill.cases");
    } break;

    case StyioASTType::Fill_If_True_Forward: {
      output += std::string("fill.only_true");
    } break;

    case StyioASTType::Fill_If_False_Forward: {
      output += std::string("fill.only_false");
    } break;

    case StyioASTType::Backward: {
      output += std::string("backward");
    } break;

    case StyioASTType::Chain_Of_Data_Processing: {
      output += std::string("chain_of_data_processing");
    } break;

    

    case StyioASTType::TypedVar: {
      output += std::string("var");
    } break;

    case StyioASTType::Pass: {
      output += std::string("do_nothing");
    } break;

    case StyioASTType::Break: {
      output += std::string("break");
    } break;

    case StyioASTType::CondFlow_True: {
      output += std::string("only_true");
    } break;

    case StyioASTType::CondFlow_False: {
      output += std::string("only_false");
    } break;

    case StyioASTType::CondFlow_Both: {
      output += std::string("if_else");
    } break;

    case StyioASTType::MainBlock: {
      output += std::string("main");
    } break;

    default: {
      output += std::string("unknown");
    } break;
  }

  return output + extra;
}

std::string
reprToken(StyioOpType token) {
  switch (token) {
    case StyioOpType::Binary_Add:
      return "<Add>";

    case StyioOpType::Binary_Sub:
      return "<Sub>";

    case StyioOpType::Binary_Mul:
      return "<Mul>";

    case StyioOpType::Binary_Div:
      return "<Div>";

    case StyioOpType::Binary_Pow:
      return "<Pow>";

    case StyioOpType::Binary_Mod:
      return "<Mod>";

    case StyioOpType::Self_Add_Assign:
      return "+=";

    case StyioOpType::Self_Sub_Assign:
      return "-=";

    case StyioOpType::Self_Mul_Assign:
      return "*=";

    case StyioOpType::Self_Div_Assign:
      return "/=";

    default:
      return "<Undefined>";
      break;
  }
}

std::string
reprToken(LogicType token) {
  switch (token) {
    case LogicType::NOT:
      return "<NOT>";

    case LogicType::AND:
      return "<AND>";

    case LogicType::OR:
      return "<OR>";

    case LogicType::XOR:
      return "<OR>";

    default:
      return "<NULL>";

      break;
  }
}

std::string
reprToken(CompType token) {
  switch (token) {
    case CompType::EQ:
      return "<EQ>";

    case CompType::NE:
      return "<NE>";

    case CompType::GT:
      return "<GT>";

    case CompType::GE:
      return "<GE>";

    case CompType::LT:
      return "<LT>";

    case CompType::LE:
      return "<LE>";

    default:
      return "<NULL>";
      break;
  }
}

StyioDataType
getMaxType(StyioDataType T1, StyioDataType T2) {
  if (T1.option == T2.option) {
    return T1;
  }

  return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
}

std::string
reprDataTypeOption(StyioDataTypeOption option) {
  switch (option) {
    case StyioDataTypeOption::Undefined: {
      return "undefined";
    } break;

    case StyioDataTypeOption::Defined: {
      return "defined";
    } break;

    case StyioDataTypeOption::Bool: {
      return "bool";
    } break;

    case StyioDataTypeOption::Integer: {
      return "int";
    } break;

    case StyioDataTypeOption::Float: {
      return "float";
    } break;

    case StyioDataTypeOption::Decimal: {
      return "decimal";
    } break;

    case StyioDataTypeOption::Char: {
      return "char";
    } break;

    case StyioDataTypeOption::String: {
      return "string";
    } break;

    case StyioDataTypeOption::Tuple: {
      return "tuple";
    } break;

    case StyioDataTypeOption::List: {
      return "list";
    } break;

    case StyioDataTypeOption::Struct: {
      return "struct";
    } break;

    case StyioDataTypeOption::Func: {
      return "func";
    } break;

    default: {
      return "unknown";
    } break;
  }
};

/*
  =========================

  StyioToken Implementation

  =========================
*/

std::string
StyioToken::getTokName(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::TOK_SPACE:
      return " ";

    case StyioTokenType::TOK_CR:
      return "<CR>";

    case StyioTokenType::TOK_LF:
      return "<LF>";

    case StyioTokenType::TOK_EOF:
      return "<EOF>";

    case StyioTokenType::NAME:
      return "<ID>";

    case StyioTokenType::INTEGER:
      return "<INT>";

    case StyioTokenType::DECIMAL:
      return "<FLOAT>";

    case StyioTokenType::STRING:
      return "<STRING>";

    case StyioTokenType::COMMENT_LINE:
      return "// COMMENT ";

    case StyioTokenType::COMMENT_CLOSED:
      return "/* COMMENT */";

    case StyioTokenType::TOK_COMMA:
      return ",";

    case StyioTokenType::TOK_DOT:
      return ".";

    case StyioTokenType::TOK_COLON:
      return ":";

    case StyioTokenType::TOK_TILDE:
      return "~";

    case StyioTokenType::TOK_EXCLAM:
      return "!";

    case StyioTokenType::TOK_AT:
      return "@";

    case StyioTokenType::TOK_HASH:
      return "#";

    case StyioTokenType::TOK_DOLLAR:
      return "$";

    case StyioTokenType::TOK_PERCENT:
      return "%";

    case StyioTokenType::TOK_HAT:
      return "^";

    case StyioTokenType::TOK_QUEST:
      return "?";

    case StyioTokenType::TOK_SLASH:
      return "/";

    case StyioTokenType::TOK_BACKSLASH:
      return "\\";

    case StyioTokenType::TOK_PIPE:
      return "|";

    case StyioTokenType::ELLIPSIS:
      return "...";

    case StyioTokenType::TOK_SQUOTE:
      return "'";

    case StyioTokenType::TOK_DQUOTE:
      return "\"";

    case StyioTokenType::TOK_BQUOTE:
      return "`";

    case StyioTokenType::TOK_LPAREN:
      return "(";

    case StyioTokenType::TOK_RPAREN:
      return ")";

    case StyioTokenType::TOK_LBOXBRAC:
      return "[";

    case StyioTokenType::TOK_RBOXBRAC:
      return "]";

    case StyioTokenType::TOK_LCURBRAC:
      return "{";

    case StyioTokenType::TOK_RCURBRAC:
      return "}";

    case StyioTokenType::TOK_EQUAL:
      return "=";

    case StyioTokenType::TOK_LANGBRAC:
      return "<";

    case StyioTokenType::TOK_RANGBRAC:
      return ">";

    case StyioTokenType::LOGIC_NOT:
      return "<NOT>";

    case StyioTokenType::LOGIC_AND:
      return "<AND>";

    case StyioTokenType::LOGIC_OR:
      return "<OR>";

    case StyioTokenType::LOGIC_XOR:
      return "<XOR>";

    case StyioTokenType::BINOP_BITAND:
      return "<BIT_AND>";

    case StyioTokenType::BINOP_BITOR:
      return "<BIT_OR>";

    case StyioTokenType::BINOP_BITXOR:
      return "<BIT_XOR>";

    case StyioTokenType::EXTRACTOR:
      return "<<";

    case StyioTokenType::ITERATOR:
      return ">>";

    case StyioTokenType::PRINT:
      return ">_";

    case StyioTokenType::UNARY_NEG:
      return "<NEG>";

    case StyioTokenType::BINOP_ADD:
      return "<ADD>";

    case StyioTokenType::BINOP_SUB:
      return "<SUB>";

    case StyioTokenType::BINOP_MUL:
      return "<MUL>";

    case StyioTokenType::BINOP_DIV:
      return "<DIV>";

    case StyioTokenType::BINOP_MOD:
      return "<MOD>";

    case StyioTokenType::BINOP_POW:
      return "<POW>";

    case StyioTokenType::BINOP_GT:
      return "<GT>";

    case StyioTokenType::BINOP_GE:
      return "<GE>";

    case StyioTokenType::BINOP_LT:
      return "<LT>";

    case StyioTokenType::BINOP_LE:
      return "<LE>";

    case StyioTokenType::BINOP_EQ:
      return "<EQ>";

    case StyioTokenType::BINOP_NE:
      return "<NE>";

    case StyioTokenType::ARROW_DOUBLE_RIGHT:
      return "=>";

    case StyioTokenType::ARROW_DOUBLE_LEFT:
      return "<=";

    case StyioTokenType::ARROW_SINGLE_RIGHT:
      return "->";

    case StyioTokenType::ARROW_SINGLE_LEFT:
      return "<-";

    case StyioTokenType::WALRUS:
      return ":=";

    case StyioTokenType::MATCH:
      return "?=";

    case StyioTokenType::INFINITE_LIST:
      return "[...]";

    default:
      return "<UNKNOWN>";
  }
};

size_t
StyioToken::length() {
  return original.length();
}

std::string
StyioToken::as_str() {
  if (type == StyioTokenType::TOK_LF) {
    return "<LF>";
  }
  else if (type == StyioTokenType::TOK_SPACE) {
    return "<SPACE>";
  }
  else if (type == StyioTokenType::NAME
           || type == StyioTokenType::INTEGER
           || type == StyioTokenType::DECIMAL) {
    return getTokName(this->type) + " = " + this->original;
  }
  else if (type == StyioTokenType::STRING) {
    return "\"" + this->original + "\"";
  }
  else {
    return getTokName(this->type);
  }
}