#include <string>

#include "../StyioToken/Token.hpp"

std::string reprStyioType (
  StyioType type,
  bool colorful,
  std::string extra) {
  int titleColor = 96;
  int flexColor = 91;
  int fixColor = 92;

  std::string output = "";

  switch (type)
  {
  case StyioType::Id:
    {
      auto name = std::string("id");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }

    break;

  case StyioType::Int:
    {
      auto name = std::string("int");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::Float:
    {
      auto name = std::string("float");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::Char:
    {
      auto name = std::string("char");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::String:
    {
      auto name = std::string("String");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::ExtPath:
    {
      auto name = std::string("Path");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::ExtLink:
    {
      auto name = std::string("Link");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::ExtPack:
    {
      auto name = std::string("Package");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::Filling:
    {
      auto name = std::string("Filling");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::Condition:
    {
      auto name = std::string("Condition");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::WriteStdOut:
    {
      auto name = std::string("Print");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::ReadFile:
    {
      auto name = std::string("Read File");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::Call:
    {
      auto name = std::string("Call");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::BinOp:
    {
      auto name = std::string("BinOp");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::ListOp:
    {
      auto name = std::string("ListOp");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::Return:
    {
      auto name = std::string("Return");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::Range:
    {
      auto name = std::string("Range");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;
  
  case StyioType::List:
    {
      auto name = std::string("List");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::Resources:
    {
      auto name = std::string("Resources");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;


    case StyioType::MutAssign:
    {
      if (colorful) {
        output = make_colorful(std::string("Binding"), titleColor) + " " + make_colorful(std::string("(Flexible)"), flexColor);
      } else {
        output = std::string("Binding (Flexible)");
      }
    }
    
    break;

  case StyioType::FixAssign:
    {
      if (colorful) {
        output = make_colorful(std::string("Binding"), titleColor) + " " + make_colorful(std::string("(Final)"), flexColor);
      } else {
        output = std::string("Binding (Final)");
      }
    }
    
    break;

  case StyioType::Block:
    {
      auto name = std::string("Block");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::CondFlow:
    {
      auto name = std::string("If Then Else");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case StyioType::Function:
    {
      auto name = std::string("func");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

case StyioType::Structure:
    {
      auto name = std::string("struct");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

case StyioType::Loop:
    {
      auto name = std::string("loop");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

case StyioType::CheckEq:
    {
      auto name = std::string("if equal?");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

case StyioType::CheckIsin:
    {
      auto name = std::string("if isin?");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

case StyioType::CheckCond:
    {
      auto name = std::string("if cond?");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  default:
    output = std::string("Styio[Unknown]");

    break;
  }
  
  return output + extra;
}

std::string reprListOp(ListOpType listOp) {
  switch (listOp)
  {
    case ListOpType::Access_Via_Name:
      return "Access_Via_Name";

    case ListOpType::Get_Reversed:
      return "Get_Reversed";

    case ListOpType::Get_Index_By_Item:
      return "Get_Index_By_Item";

    case ListOpType::Insert_Item_By_Index:
      return "Insert_Item_By_Index";

    case ListOpType::Remove_Item_By_Index:
      return "Remove_Item_By_Index";

    case ListOpType::Remove_Item_By_Value:
      return "Remove_Item_By_Value";

    case ListOpType::Remove_Many_Items_By_Indices:
      return "Remove_Many_Items_By_Indices";

    case ListOpType::Remove_Many_Items_By_Values:
      return "Remove_Many_Items_By_Values";

    default:
      return "List_Operation";

      break;
  }
}

std::string reprFlow (FlowType flow) {
  switch (flow)
  {
    case FlowType::True:
      return "True";

    case FlowType::False:
      return "False";

    case FlowType::Both:
      return "True & False";

    default:
      return "Control Flow";

      break;
  }
}

std::string reprToken(LogicType token) {
  switch (token)
  {
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

std::string reprToken(CompType token) {
  switch (token)
  {
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

std::string reprToken(BinOpType token) {
  switch (token)
  {
    case BinOpType::BIN_ADD:
      return "<ADD>";

    case BinOpType::BIN_SUB:
      return "<SUB>";

    case BinOpType::BIN_MUL:
      return "<MUL>";

    case BinOpType::BIN_DIV:
      return "<DIV>";

    case BinOpType::BIN_POW:
      return "<POW>";

    case BinOpType::BIN_MOD:
      return "<DIV>";

    default:
      return "<NULL>";

      break;
  }
};


std::string reprToken(StyioToken token) {
  switch (token)
  {
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