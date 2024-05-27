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

    case StyioASTType::Variable: {
      output += std::string("var");
    } break;

    case StyioASTType::Arg: {
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

    case StyioASTType::VarTuple: {
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

    case StyioASTType::Chain_Of_Data_Processing: {
      output += std::string("chain_of_data_processing");
    } break;

    case StyioASTType::DType: {
      output += std::string("type");
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
reprToken(TokenKind token) {
  switch (token) {
    case TokenKind::Binary_Add:
      return "<Add>";

    case TokenKind::Binary_Sub:
      return "<Sub>";

    case TokenKind::Binary_Mul:
      return "<Mul>";

    case TokenKind::Binary_Div:
      return "<Div>";

    case TokenKind::Binary_Pow:
      return "<Pow>";

    case TokenKind::Binary_Mod:
      return "<Mod>";

    case TokenKind::Self_Add_Assign:
      return "+=";

    case TokenKind::Self_Sub_Assign:
      return "-=";

    case TokenKind::Self_Mul_Assign:
      return "*=";

    case TokenKind::Self_Div_Assign:
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

std::string
reprToken(StyioToken token) {
  switch (token) {
    case StyioToken::TOK_SPACE:
      return " ";

    case StyioToken::TOK_CR:
      return "<CR>";

    case StyioToken::TOK_LF:
      return "<LF>";

    case StyioToken::TOK_EOF:
      return "<EOF>";

    case StyioToken::TOK_ID:
      return "<ID>";

    case StyioToken::TOK_INT:
      return "<INT>";

    case StyioToken::TOK_FLOAT:
      return "<FLOAT>";

    case StyioToken::TOK_STRING:
      return "<STRING>";

    case StyioToken::TOK_COMMA:
      return ",";

    case StyioToken::TOK_DOT:
      return ".";

    case StyioToken::TOK_COLON:
      return ":";

    case StyioToken::TOK_TILDE:
      return "~";

    case StyioToken::TOK_EXCLAM:
      return "!";

    case StyioToken::TOK_AT:
      return "@";

    case StyioToken::TOK_HASH:
      return "#";

    case StyioToken::TOK_DOLLAR:
      return "$";

    case StyioToken::TOK_PERCENT:
      return "%";

    case StyioToken::TOK_HAT:
      return "^";

    case StyioToken::TOK_CHECK:
      return "?";

    case StyioToken::TOK_SLASH:
      return "/";

    case StyioToken::TOK_BSLASH:
      return "\\";

    case StyioToken::TOK_PIPE:
      return "|";

    case StyioToken::TOK_ELLIPSIS:
      return "...";

    case StyioToken::TOK_SQUOTE:
      return "'";

    case StyioToken::TOK_DQUOTE:
      return "\"";

    case StyioToken::TOK_BQUOTE:
      return "`";

    case StyioToken::TOK_LPAREN:
      return "(";

    case StyioToken::TOK_RPAREN:
      return ")";

    case StyioToken::TOK_LBOXBRAC:
      return "[";

    case StyioToken::TOK_RBOXBRAC:
      return "]";

    case StyioToken::TOK_LCURBRAC:
      return "{";

    case StyioToken::TOK_RCURBRAC:
      return "}";

    case StyioToken::TOK_LANGBRAC:
      return "<";

    case StyioToken::TOK_RANGBRAC:
      return ">";

    case StyioToken::TOK_NOT:
      return "<NOT>";

    case StyioToken::TOK_AND:
      return "<AND>";

    case StyioToken::TOK_OR:
      return "<OR>";

    case StyioToken::TOK_XOR:
      return "<XOR>";

    case StyioToken::TOK_BITAND:
      return "<BIT_AND>";

    case StyioToken::TOK_BITOR:
      return "<BIT_OR>";

    case StyioToken::TOK_BITXOR:
      return "<BIT_XOR>";

    case StyioToken::TOK_LSHIFT:
      return "<<";

    case StyioToken::TOK_RSHIFT:
      return ">>";

    case StyioToken::TOK_NEG:
      return "<NEG>";

    case StyioToken::TOK_ADD:
      return "<ADD>";

    case StyioToken::TOK_SUB:
      return "<SUB>";

    case StyioToken::TOK_MUL:
      return "<MUL>";

    case StyioToken::TOK_DIV:
      return "<DIV>";

    case StyioToken::TOK_MOD:
      return "<MOD>";

    case StyioToken::TOK_POW:
      return "<POW>";

    case StyioToken::TOK_GT:
      return "<GT>";

    case StyioToken::TOK_GE:
      return "<GE>";

    case StyioToken::TOK_LT:
      return "<LT>";

    case StyioToken::TOK_LE:
      return "<LE>";

    case StyioToken::TOK_EQ:
      return "<EQ>";

    case StyioToken::TOK_NE:
      return "<NE>";

    case StyioToken::TOK_RARROW:
      return "->";

    case StyioToken::TOK_LARROW:
      return "<-";

    case StyioToken::TOK_WALRUS:
      return ":=";

    case StyioToken::TOK_MATCH:
      return "?=";

    case StyioToken::TOK_INFINITE_LIST:
      return "[...]";

    default:
      return "<UNKNOWN>";
  }
};

StyioDataType
getMaxType(StyioDataType T1, StyioDataType T2) {
  if (T1.option == T2.option) {
    return T1;
  }

  return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
}