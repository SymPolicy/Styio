
#include <string>
#include <vector>
#include <iostream>

#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"
#include "../StyioUtil/Util.hpp"
#include "Parser.hpp"

/*
  =================
*/

int get_next_char() 
{
  return getchar();
}

void get_next_char(int& cur_char)
{
  cur_char = getchar();
}

bool check_this_char(int& cur_char, char value)
{
  return cur_char == value;
}

void drop_all_spaces (int& cur_char) 
{
  while (isspace(cur_char)) {
    cur_char = get_next_char();
  };
}

void drop_white_spaces (int& cur_char) 
{
  while (check_this_char(cur_char, ' ')) {
    cur_char = get_next_char();
  };
}

/*
  =================
*/


IdAST* parse_id (std::vector<int>& tok_ctx, int& cur_char)
{
  std::string idStr = "";
  idStr += cur_char;

  // [a-zA-Z][a-zA-Z0-9_]*
  while (isalnum((cur_char = get_next_char())) || check_this_char(cur_char, '_')) 
  {
    idStr += cur_char;
  }

  return new IdAST(idStr);
}

IntAST* parse_int (std::vector<int>& tok_ctx, int& cur_char)
{
  std::string intStr = "";

  // push the current character into string
  intStr += cur_char;

  // progress to the next
  cur_char = get_next_char();

  // [0-9]*
  while (isdigit(cur_char))
  {
    intStr += cur_char;
    cur_char = get_next_char();
  };

  return new IntAST(std::stoi(intStr));
}

StyioAST* parse_int_or_float (std::vector<int>& tok_ctx, int& cur_char)
{
  std::string numStr = "";
  numStr += cur_char;
  cur_char = get_next_char();

  // [0-9]*
  while (isdigit(cur_char))
  {
    numStr += cur_char;
    cur_char = get_next_char();
  };

  if (check_this_char(cur_char, '.')) 
  {
    cur_char = get_next_char();

    if (isdigit(cur_char))
    {
      numStr += '.';

      numStr += cur_char;

      while (isdigit(cur_char))
      {
        numStr += cur_char;
        cur_char = get_next_char();
      };

      return new FloatAST(std::stod(numStr));
    }
    else
    {
      return new IntAST(std::stoi(numStr));
    };
  } 
  else 
  {
    return new IntAST(std::stoi(numStr));
  }
}

StringAST* parse_string (
  std::vector<int>& tok_ctx, 
  int& cur_char
)
{
  // eliminate the first(start) double quote
  cur_char = get_next_char();

  std::string textStr = "";
  
  while (cur_char != '\"')
  {
    textStr += cur_char;
    cur_char = get_next_char();
  };

  // eliminate the second(end) double quote
  cur_char = get_next_char();

  return new StringAST(textStr);
}

StyioAST* parse_char_or_string (
  std::vector<int>& tok_ctx, 
  int& cur_char
)
{
  // eliminate the first(start) single quote
  get_next_char(cur_char);

  std::string textStr = "";
  
  while (cur_char != '\'')
  {
    textStr += cur_char;
    get_next_char(cur_char);
  };

  // eliminate the second(end) single quote
  get_next_char(cur_char);

  if (textStr.length() == 1)
  {
    return new CharAST(textStr);
  }
  else
  {
    return new StringAST(textStr);
  }
}

SizeOfAST* parse_size_of (std::vector<int>& tok_ctx, int& cur_char) 
{
  cur_char = get_next_char();
       
  if (isalpha(cur_char) || check_this_char(cur_char, '_'))
  {
    IdAST* var = parse_id(tok_ctx, cur_char);

    if (check_this_char(cur_char, '|')) {
      cur_char = get_next_char();

      return new SizeOfAST(var);
    }
    else
    {
      std::string errmsg = std::string("Expecting | at the end of SizeOf(), but got `") + char(cur_char) + "`";
      throw StyioSyntaxError(errmsg);
    }
  }
  else
  {
    std::string errmsg = std::string("Unexpected SizeOf(), starts with `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  }
}

/*
  =================
*/


StyioAST* parse_ext_res (std::vector<int>& tok_ctx, int& cur_char)
{
  // eliminate @
  cur_char = get_next_char();

  if (check_this_char(cur_char, '(')) {
    // eliminate (
    cur_char = get_next_char();

    if (check_this_char(cur_char, '\"')) {
      // eliminate the left double quote "
      cur_char = get_next_char();

      std::string textStr = "";
  
      while (cur_char != '\"')
      {
        textStr += cur_char;
        cur_char = get_next_char();
      };

      if (check_this_char(cur_char, '\"')) {
        // eliminate the right double quote "
        cur_char = get_next_char();
      }
      else
      {
        std::string errmsg = std::string("Expecting \" at the end, but got `") + char(cur_char) + "`.";
        throw StyioSyntaxError(errmsg);
      };

      if (check_this_char(cur_char, ')')) {
        // eliminate )
        cur_char = get_next_char();
      }
      else
      {
        std::string errmsg = std::string("Expecting ) at the end, but got `") + char(cur_char) + "`.";
        throw StyioSyntaxError(errmsg);
      };

      return new ExtPathAST(textStr);
    }
    else
    {
      std::string errmsg = std::string("Unexpected external resource, starts with `") + char(cur_char) + "`.";
      throw StyioSyntaxError(errmsg);
    }
  }
  else
  {
    std::string errmsg = "External resource must be wrapped with `(` and `)`.";
    throw StyioSyntaxError(errmsg);
  };
}

StyioAST* parse_list_elem (std::vector<int>& tok_ctx, int& cur_char)
{
  if (isdigit(cur_char)) 
  {
    return parse_int_or_float(tok_ctx, cur_char);
  }
  else if (isalpha(cur_char) || check_this_char(cur_char, '_')) 
  {
    return parse_id(tok_ctx, cur_char);
  }
  else if (check_this_char(cur_char, '\"')) 
  {
    return parse_string(tok_ctx, cur_char);
  }
  
  std::string errmsg = std::string("Unexpected List / Range Element, starts with character `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

ListOpAST* parse_list_op (
  std::vector<int>& tok_ctx, 
  int& cur_char,
  StyioAST* theList
) 
{
  // eliminate [ at the start
  cur_char = get_next_char();

  ListOpAST* listop;

  switch (cur_char)
  {
  // List.get_reversed()
  case '<':
    {
      cur_char = get_next_char();

      listop = new ListOpAST(
        theList, 
        ListOpType::Reversed);
    }

    // You should NOT reach this line!
    break;

  // List.get_item_by_list(item)
  case '?':
    {
      cur_char = get_next_char();

      if (check_this_char(cur_char, '='))
      {
        cur_char = get_next_char();

        StyioAST* theItem = parse_list_elem(tok_ctx, cur_char);

        listop = new ListOpAST(
          theList, 
          ListOpType::Get_Index_By_Item,
          theItem);
      }
      else
      {
        std::string errmsg = std::string("Missing `=` for `?=` after `?= item`, but got `") + char(cur_char) + "`";
        throw StyioSyntaxError(errmsg);
      };
    }

    // You should NOT reach this line!
    break;
  
  // List.insert_item_by_index(index, item)
  case '^':
    {
      cur_char = get_next_char();

      // eliminate white spaces between ^ and index
      drop_white_spaces(cur_char);

      StyioAST* theIndex = parse_int(tok_ctx, cur_char);

      // eliminate white spaces between index and <-
      drop_white_spaces(cur_char);

      if (check_this_char(cur_char, '<'))
      {
        cur_char = get_next_char();

        if (check_this_char(cur_char, '-'))
        {
          cur_char = get_next_char();

          // eliminate white spaces between <- and the value to be inserted
          drop_white_spaces(cur_char);

          // the item to be inserted into the list
          StyioAST* theItemIns = parse_list_elem(tok_ctx, cur_char);

          listop = new ListOpAST(
            theList, 
            ListOpType::Insert_Item_By_Index,
            theIndex,
            theItemIns);
        }
        else
        {
          std::string errmsg = std::string("Missing `-` for `<-` after `^? index`, but got `") + char(cur_char) + "`";
          throw StyioSyntaxError(errmsg);
        };
      }
      else
      {
        std::string errmsg = std::string("Expecting `<-` after `^? index`, but got `") + char(cur_char) + "`";
        throw StyioSyntaxError(errmsg);
      }
    }

    // You should NOT reach this line!
    break;
  
  default:
    {
      std::string errmsg = std::string("Unexpected List[Operation], starts with ") + char(cur_char);
      throw StyioSyntaxError(errmsg);
    }

    // You should NOT reach this line!
    break;
  }

  // check [ at the end
  if (check_this_char(cur_char, ']'))
  {
    // eliminate [ at the end
    cur_char = get_next_char();
    
    return listop;
  }
  else
  {
    std::string errmsg = std::string("Missing `]` after List[Operation].");
    throw StyioSyntaxError(errmsg);
  };
}

std::vector<IdAST*> parse_multi_vars (
  std::vector<int>& tok_ctx, 
  int& cur_char
) 
{
  std::vector<IdAST*> vars;

  IdAST* firstVar = parse_id(tok_ctx, cur_char);

  vars.push_back(firstVar);

  drop_white_spaces(cur_char);

  while (check_this_char(cur_char, ','))
  {
    cur_char = get_next_char();

    drop_white_spaces(cur_char);

    if (check_this_char(cur_char, ')')  || check_this_char(cur_char, '|'))
    {
      break;
    };

    vars.push_back(parse_id(tok_ctx, cur_char));
  }

  return vars;
}

StyioAST* parse_iter_over (
  std::vector<int>& tok_ctx, 
  int& cur_char,
  StyioAST* iterOverIt) 
{
  std::vector<IdAST*> iterTmpVars;
  StyioAST* iterBlock;

  // eliminate the start
  switch (cur_char)
  {
  case '(':
    cur_char = get_next_char();
    break;

  case '[':
    cur_char = get_next_char();
    break;

  case '{':
    cur_char = get_next_char();
    break;

  case '|':
    cur_char = get_next_char();
    break;
  
  default:
    break;
  }

  drop_white_spaces(cur_char);

  iterTmpVars = parse_multi_vars(tok_ctx, cur_char);

  drop_white_spaces(cur_char);

  // eliminate the end
  switch (cur_char)
  {
  case ')':
    cur_char = get_next_char();
    break;

  case ']':
    cur_char = get_next_char();
    break;

  case '}':
    cur_char = get_next_char();
    break;

  case '|':
    cur_char = get_next_char();
    break;
  
  default:
    break;
  }

  get_next_char(cur_char);

  if (check_this_char(cur_char, '?'))
  {
    get_next_char(cur_char);

    if (check_this_char(cur_char, '='))
    {
      get_next_char(cur_char);

      StyioAST* checkWithEl = parse_simple_value(tok_ctx, cur_char);

      // TODO: Match Block
    }
    else
    {
      std::string errmsg = std::string("Missing `=` for `?=`.");
      throw StyioSyntaxError(errmsg);
    };
  };

  if (check_this_char(cur_char, '='))
  {
    get_next_char(cur_char);

    if (check_this_char(cur_char, '>'))
    {
      get_next_char(cur_char);

      // support newline(\n) and other white spaces
      drop_all_spaces(cur_char);

      if (check_this_char(cur_char, '{'))
      {
        iterBlock = parse_block(tok_ctx, cur_char);
      }
      else
      {
        std::string errmsg = std::string("Cannot find block after `=>`.");
        throw StyioSyntaxError(errmsg);
      };
    }
    else
    {
      std::string errmsg = std::string("Missing `>` for `=>`.");
      throw StyioSyntaxError(errmsg);
    };
  };

  switch (iterOverIt -> hint())
  {
  case StyioType::InfLoop:
    return new IterInfLoopAST(iterTmpVars, iterBlock);

    // You should NOT reach this line!
    break;

  case StyioType::List:
    return new IterListAST(iterOverIt, iterTmpVars, iterBlock);

    // You should NOT reach this line!
    break;

  case StyioType::Range:
    return new IterRangeAST(iterOverIt, iterTmpVars, iterBlock);

    // You should NOT reach this line!
    break;
  
  default:
    {
      std::string errmsg = std::string("Cannot recognize the collection for the iterator: ") + std::to_string(type_to_int(iterOverIt -> hint()));
      throw StyioSyntaxError(errmsg);
    }

    // You should NOT reach this line!
    break;
  }
}

StyioAST* parse_list_expr (std::vector<int>& tok_ctx, int& cur_char) 
{
  std::vector<StyioAST*> elements;

  StyioAST* startEl = parse_list_elem(tok_ctx, cur_char);
  elements.push_back(startEl);

  drop_white_spaces(cur_char);

  switch (cur_char)
  {
  case '.':
    {
      cur_char = get_next_char();

      while (check_this_char(cur_char, '.'))
      {
        cur_char = get_next_char();
      }
      
      StyioAST* endEl = parse_list_elem(tok_ctx, cur_char);

      StyioAST* list_loop;

      if (startEl -> hint() == StyioType::Int 
        && endEl -> hint() == StyioType::Id)
      {
        list_loop = new InfLoopAST(startEl, endEl);
      }
      else
      if (startEl -> hint() == StyioType::Int 
        && endEl -> hint() == StyioType::Int)
      {
        list_loop = new RangeAST(startEl, endEl, new IntAST(1));
      }
      else
      {
        std::string errmsg = std::string("Unexpected Range / List / Loop: ")
          + "Start <"
          + std::to_string(type_to_int(startEl -> hint()))
          + ">, "
          + "End <"
          + std::to_string(type_to_int(endEl -> hint()))
          + ">.";
        throw StyioSyntaxError(errmsg);
      }

      if (check_this_char(cur_char, ']')) 
      {
        cur_char = get_next_char();
      }
      else
      {
        std::string errmsg = std::string("Missing `]` after List / Range / Loop: `") + char(cur_char) + "`";
        throw StyioSyntaxError(errmsg);
      };

      drop_white_spaces(cur_char);

      switch (cur_char)
      {
      case '\n':
        {
          // If: LF, Then: Statement Ends
          return list_loop;
        }
        
        // You should NOT reach this line!
        break;

      case '>':
        {
          cur_char = get_next_char();

          if (check_this_char(cur_char, '>'))
          {
            // If: >>, Then: Iteration
            cur_char = get_next_char();
            
            return parse_iter_over(tok_ctx, cur_char, list_loop);
          }
        }
        
        // You should NOT reach this line!
        break;

      case '[':
        {
          return parse_list_op(tok_ctx, cur_char, list_loop);
        }
        
        // You should NOT reach this line!
        break;
      
      default:
        {
          std::string errmsg = std::string("Unexpected character after List / Range / Loop: `") + char(cur_char) + "`";
          throw StyioSyntaxError(errmsg);
        };
        
        // You should NOT reach this line!
        break;
      }
    }

    // You should not reach this line!
    break;

  case ',':
    {
      ListAST* theList;

      while (check_this_char(cur_char, ','))
      {
        // eliminate ,
        cur_char = get_next_char();

        drop_white_spaces(cur_char);

        if (check_this_char(cur_char, ']')) 
        {
          cur_char = get_next_char();

          theList = new ListAST(elements);
        };

        StyioAST* el = parse_list_elem(tok_ctx, cur_char);

        elements.push_back(el);
      };

      drop_white_spaces(cur_char);

      if (check_this_char(cur_char, ']')) 
      {
        cur_char = get_next_char();

        theList = new ListAST(elements);
      }
      else
      {
        std::string errmsg = std::string("Missing `]` after List / Range / Loop: `") + char(cur_char) + "`";
        throw StyioSyntaxError(errmsg);
      };

      drop_white_spaces(cur_char);

      switch (cur_char)
      {
      case '\n':
        {
          // If: LF, Then: Statement Ends
          return theList;
        }
        
        // You should NOT reach this line!
        break;

      case '>':
        {
          cur_char = get_next_char();

          if (check_this_char(cur_char, '>'))
          {
            // If: >>, Then: Iteration
            cur_char = get_next_char();

            
          }

          // TODO: Iteration Over List / Range / Loop

          return theList;
        }
        
        // You should NOT reach this line!
        break;

      case '[':
        {
          return parse_list_op(tok_ctx, cur_char, theList);
        }
        
        // You should NOT reach this line!
        break;
      
      default:
        {
          std::string errmsg = std::string("Unexpected character after List / Range / Loop: `") + char(cur_char) + "`";
          throw StyioSyntaxError(errmsg);
        };
        
        // You should NOT reach this line!
        break;
      }
    }

    // You should not reach this line!
    break;
  
  default:
    break;
  }

  std::string errmsg = std::string("Uncompleted List, ends with character `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

InfLoopAST* parse_loop (std::vector<int>& tok_ctx, int& cur_char)
{
  while (check_this_char(cur_char, '.')) 
  { 
    // eliminate all .
    cur_char = get_next_char();

    if (check_this_char(cur_char, ']')) 
    {
      cur_char = get_next_char();

      return new InfLoopAST();
    };
  };

  if (isdigit(cur_char))
  {
    StyioAST* errnum = parse_int_or_float(tok_ctx, cur_char);
    
    std::string errmsg = std::string("A finite list must have both start and end values. However, only the end value is detected: `") + errnum -> toStringInline() + "`. Try `[0.." + errnum -> toStringInline() + "]` rather than `[.." + errnum -> toStringInline() + "]`.";
    throw StyioSyntaxError(errmsg);
  }

  std::string errmsg = std::string("Unexpected character `") + char(cur_char) + "` in infinite expression.";
  throw StyioSyntaxError(errmsg);
}

StyioAST* parse_bin_rhs (
  std::vector<int>& tok_ctx, 
  int& cur_char
) 
{
  drop_white_spaces(cur_char);

  // ID
  if (isalpha(cur_char) || check_this_char(cur_char, '_')) {
    IdAST* result = parse_id(tok_ctx, cur_char);
    return result;
  }
  else
  // Int / Float
  if (isdigit(cur_char)) {
    StyioAST* result = parse_int_or_float(tok_ctx, cur_char);
    return result;
  }
  else
  {
    switch (cur_char)
    {
    // List
    case '[':
      {
        cur_char = get_next_char();

        if (check_this_char(cur_char, ']')) {
          cur_char = get_next_char();

          return new EmptyListAST();
        }
        else
        {
          return parse_list_expr(tok_ctx, cur_char);
        }
      }

      // You should NOT reach this line!
      break;

    // SizeOf()
    case '|':
      {
        return parse_size_of(tok_ctx, cur_char);
      }

      // You should NOT reach this line!
      break;
    
    default:
      break;
    }
  };

  std::string errmsg = std::string("Unexpected BinOp.RHS, starts with `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

BinOpAST* parse_bin_op (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  StyioAST* lhs_ast
) 
{
  BinOpAST* binOp;

  drop_white_spaces(cur_char);

  switch (cur_char)
  {
    // BIN_ADD := <ID> "+" <EXPR>
    case '+':
      {
        cur_char = get_next_char();

        // <ID> "+" |-- 
        binOp = new BinOpAST(BinOpType::BIN_ADD, lhs_ast, parse_bin_rhs(tok_ctx, cur_char));
      };

      // You should NOT reach this line!
      break;

    // BIN_SUB := <ID> "-" <EXPR>
    case '-':
      {
        cur_char = get_next_char();

        // <ID> "-" |--
        binOp = new BinOpAST(BinOpType::BIN_SUB, lhs_ast, parse_bin_rhs(tok_ctx, cur_char));
      };

      // You should NOT reach this line!
      break;

    // BIN_MUL | BIN_POW
    case '*':
      {
        cur_char = get_next_char();
        // BIN_POW := <ID> "**" <EXPR>
        if (check_this_char(cur_char, '*'))
        {
          cur_char = get_next_char();

          // <ID> "**" |--
          binOp = new BinOpAST(BinOpType::BIN_POW, lhs_ast, parse_bin_rhs(tok_ctx, cur_char));
        } 
        // BIN_MUL := <ID> "*" <EXPR>
        else 
        {
          // <ID> "*" |--
          binOp = new BinOpAST(BinOpType::BIN_MUL, lhs_ast, parse_bin_rhs(tok_ctx, cur_char));
        }
      };
      // You should NOT reach this line!
      break;
      
    // BIN_DIV := <ID> "/" <EXPR>
    case '/':
      {
        cur_char = get_next_char();

        // <ID> "/" |-- 
        binOp = new BinOpAST(BinOpType::BIN_DIV, lhs_ast, parse_bin_rhs(tok_ctx, cur_char));
      };

      // You should NOT reach this line!
      break;

    // BIN_MOD := <ID> "%" <EXPR> 
    case '%':
      {
        cur_char = get_next_char();

        // <ID> "%" |-- 
        binOp = new BinOpAST(BinOpType::BIN_MOD, lhs_ast, parse_bin_rhs(tok_ctx, cur_char));
      };

      // You should NOT reach this line!
      break;
    
    default:
      std::string errmsg = std::string("Unexpected BinOp.Operator: `") + char(cur_char) + "`.";
      throw StyioSyntaxError(errmsg);
  }

  while (cur_char != '\n') 
  {
    binOp = parse_bin_op(tok_ctx, cur_char, binOp);
  }

  return binOp;
}

StyioAST* parse_simple_value (
  std::vector<int>& tok_ctx, 
  int& cur_char
)
{
  if (isdigit(cur_char)) 
  {
    return parse_int_or_float(tok_ctx, cur_char);
  }
  else 
  if (isalpha(cur_char) || check_this_char(cur_char, '_')) 
  {
    return parse_id(tok_ctx, cur_char);
  }
  else 
  if (check_this_char(cur_char, '\"')) 
  {
    return parse_string(tok_ctx, cur_char);
  }
  else
  if (check_this_char(cur_char, '\'')) 
  {
    return parse_char_or_string(tok_ctx, cur_char);
  }
  else
  {
    std::string errmsg = std::string("Unexpected character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

StyioAST* parse_value_expr (
  std::vector<int>& tok_ctx, 
  int& cur_char
)
{
  // <ID>
  if (isalpha(cur_char) || check_this_char(cur_char, '_')) 
  {
    // parse id
    IdAST* id_ast = parse_id(tok_ctx, cur_char);
    
    // ignore white spaces after id
    drop_white_spaces(cur_char);

    if (is_bin_tok(cur_char))
    {
      return parse_bin_op(tok_ctx, cur_char, id_ast);
    }
    else
    {
      return id_ast;
    }
  }
  else
  if (isdigit(cur_char)) {
    StyioAST* numAST = parse_int_or_float(tok_ctx, cur_char);

    // ignore white spaces after number
    drop_white_spaces(cur_char);

    if (is_bin_tok(cur_char))
    {
      return parse_bin_op(tok_ctx, cur_char, numAST);
    }
    else
    {
      return numAST;
    }
  }
  else
  {
    switch (cur_char)
    {
    case '[':
      {
        cur_char = get_next_char();

        drop_white_spaces(cur_char);

        if (check_this_char(cur_char, ']')) {
          cur_char = get_next_char();

          return new EmptyListAST();
        }
        else
        {
          return parse_list_expr(tok_ctx, cur_char);
        }
      }

      // You should NOT reach this line!
      break;

    case '|':
      {
        SizeOfAST* valExpr = parse_size_of(tok_ctx, cur_char);

        drop_white_spaces(cur_char);

        if (is_bin_tok(cur_char))
        {
          return parse_bin_op(tok_ctx, cur_char, valExpr);
        }
        else
        {
          return valExpr;
        };
      }

      // You should NOT reach this line!
      break;
    
    default:
      break;
    }
  };

  return new NoneAST();
}

MutAssignAST* parse_mut_assign (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  IdAST* id_ast
)
{
  MutAssignAST* output = new MutAssignAST(id_ast, parse_value_expr(tok_ctx, cur_char));
  
  if (check_this_char(cur_char, '\n')) 
  {
    return output;
  }
  else
  {
    std::string errmsg = std::string("Unexpected character `") + char(cur_char) + "` after Assign(Mutable)";
    throw StyioSyntaxError(errmsg);
  }
}

FixAssignAST* parse_fix_assign (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  IdAST* id_ast
) 
{
  FixAssignAST* output = new FixAssignAST(id_ast, parse_value_expr(tok_ctx, cur_char));
  
  if (check_this_char(cur_char, '\n')) 
  {
    return output;
  }
  else
  {
    std::string errmsg = std::string("Unexpected character `") + char(cur_char) + "` after Assign(Mutable)";
    throw StyioSyntaxError(errmsg);
  }
}

StyioAST* parse_read_file (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  IdAST* id_ast
) 
{
  if (check_this_char(cur_char, '@'))
  {
    StyioAST* value = parse_ext_res(tok_ctx, cur_char);

    return new ReadFileAST(id_ast, value);
  }
  else
  {
    std::string errmsg = std::string("Unexpected Read.Path, starts with character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

StyioAST* parse_stmt (std::vector<int>& tok_ctx, int& cur_char) 
{
  drop_white_spaces(cur_char);

  // <ID>
  if (isalpha(cur_char) || check_this_char(cur_char, '_')) 
  {
    // parse id
    IdAST* id_ast = parse_id(tok_ctx, cur_char);
    
    // ignore white spaces after id
    drop_white_spaces(cur_char);

    // check next character
    switch (cur_char)
    {
      // <LF>
      case '\n':
        {
          return id_ast;
        };

        // You should NOT reach this line!
        break;

      // <ID> = <EXPR>
      case '=':
        {
          cur_char = get_next_char();

          drop_white_spaces(cur_char);

          // <ID> = |--
          return parse_mut_assign(tok_ctx, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;
      
      // <ID> := <EXPR>
      case ':':
        {
          cur_char = get_next_char();
          if (check_this_char(cur_char, '='))
          {
            cur_char = get_next_char();

            drop_white_spaces(cur_char);
            
            // <ID> := |--
            return parse_fix_assign(tok_ctx, cur_char, id_ast);
          }
          else
          {
            std::string errmsg = std::string("Unexpected `:`");
            throw StyioSyntaxError(errmsg);
          }
        };

        // You should NOT reach this line!
        break;

      // <ID> <- <EXPR>
      case '<':
        {
          // eliminate <
          cur_char = get_next_char();

          if (check_this_char(cur_char, '-'))
          {
            // eliminate -
            cur_char = get_next_char();

            drop_white_spaces(cur_char);
            
            // <ID> <- |--
            return parse_read_file(tok_ctx, cur_char, id_ast);
          }
          else
          {
            std::string errmsg = std::string("Expecting `-` after `<`, but found `") + char(cur_char) + "`.";
            throw StyioSyntaxError(errmsg);
          }
        };

        // You should NOT reach this line!
        break;

      // BIN_ADD := <ID> "+" <EXPR>
      case '+':
        {
          // <ID> |-- 
          return parse_bin_op(tok_ctx, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;

      // BIN_SUB := <ID> "-" <EXPR>
      case '-':
        {
          // <ID> |--
          return parse_bin_op(tok_ctx, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;

      // BIN_MUL | BIN_POW
      case '*':
        {
          // <ID> |--
          return parse_bin_op(tok_ctx, cur_char, id_ast);
        };
        // You should NOT reach this line!
        break;
        
      // BIN_DIV := <ID> "/" <EXPR>
      case '/':
        {
          // <ID> |-- 
          return parse_bin_op(tok_ctx, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;

      // BIN_MOD := <ID> "%" <EXPR> 
      case '%':
        {
          // <ID> |-- 
          return parse_bin_op(tok_ctx, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;
      
      default:
        break;
    }
  }

  if (isdigit(cur_char)) {
    StyioAST* numAST = parse_int_or_float(tok_ctx, cur_char);

    drop_all_spaces(cur_char);

    switch (cur_char)
    {
      // <LF>
      case '\n':
        {
          return numAST;
        };

        // You should NOT reach this line!
        break;

      // BIN_ADD := [<Int>|<Float>] "+" <EXPR>
      case '+':
        {
          // [<Int>|<Float>] |--
          BinOpAST* bin_ast = parse_bin_op(tok_ctx, cur_char, numAST);
          return bin_ast;
        };

        // You should NOT reach this line!
        break;

      // BIN_SUB := [<Int>|<Float>] "-" <EXPR>
      case '-':
        {
          // [<Int>|<Float>] |--
          BinOpAST* bin_ast = parse_bin_op(tok_ctx, cur_char, numAST);
          return bin_ast;
        };

        // You should NOT reach this line!
        break;

      // BIN_MUL | BIN_POW
      case '*':
        {
          // [<Int>|<Float>] |--
          BinOpAST* bin_ast = parse_bin_op(tok_ctx, cur_char, numAST);
          return bin_ast;
        }

        // You should NOT reach this line!
        break;

      // BIN_DIV := [<Int>|<Float>] "/" <EXPR>
      case '/':
        {
          // [<Int>|<Float>] |--
          BinOpAST* bin_ast = parse_bin_op(tok_ctx, cur_char, numAST);
          return bin_ast;
        };

        // You should NOT reach this line!
        break;

      // BIN_MOD := [<Int>|<Float>] "%" <EXPR>
      case '%':
        {
          // [<Int>|<Float>] |--
          BinOpAST* bin_ast = parse_bin_op(tok_ctx, cur_char, numAST);
          return bin_ast;
        };

        // You should NOT reach this line!
        break;

      default:
        return numAST;

        // You should NOT reach this line!
        break;
    }
  }

  switch (cur_char)
  {
    // VAR_DEF := "@" "(" [<ID> ["," <ID>]*]? ")"
    case '@':
      {
        cur_char = get_next_char();
        
        drop_white_spaces(cur_char);

        if (check_this_char(cur_char, '(')) 
        {
          cur_char = get_next_char();

        };
        
        std::vector<IdAST*> varBuffer;

        if (isalpha(cur_char) || check_this_char(cur_char, '_')) {
          // "@" "(" |--
          IdAST* id_ast = parse_id(tok_ctx, cur_char);
      
          varBuffer.push_back(id_ast);
        };

        drop_white_spaces(cur_char);

        // "@" "(" [<ID> |--
        while (check_this_char(cur_char, ','))
        {
          cur_char = get_next_char();

          drop_white_spaces(cur_char);

          if (isalpha(cur_char) || check_this_char(cur_char, '_')) {
            
            IdAST* id_ast = parse_id(tok_ctx, cur_char);
      
            varBuffer.push_back(id_ast);

            drop_white_spaces(cur_char);
          };
        };
        
        if (check_this_char(cur_char, ')')) 
        {
          cur_char = get_next_char();
        };

        VarDefAST* varDef = new VarDefAST(varBuffer);

        return varDef;
        
        // You should NOT reach this line!
        break;
      };

    case ':':
      cur_char = get_next_char();

      if (check_this_char(cur_char, '=')) {
        cur_char = get_next_char();
      } 
      else
      {

      };
      
      break;

    case '-':
      cur_char = get_next_char();

      if (check_this_char(cur_char, '>')) {
        cur_char = get_next_char();
      };
      
      break;

    case '?':
      cur_char = get_next_char();
      
      if (check_this_char(cur_char, '=')) {
        cur_char = get_next_char();
      } 
      else 
      {

      };
      
      break;
    
    case '\"':
      {
        return parse_string(tok_ctx, cur_char);
      }

      
      break;

    case '!':
      {
        cur_char = get_next_char();
        
        if (check_this_char(cur_char, '~')) {
          cur_char = get_next_char();
        };
      };

      // You should NOT reach this line!
      break;

    case '(':
      cur_char = get_next_char();
      break;

    case '[':
      {
        cur_char = get_next_char();

        if (check_this_char(cur_char, '.')) {
          // eliminate the first dot .
          cur_char = get_next_char();

          if (check_this_char(cur_char, ']')) 
          {
            std::string errmsg = std::string("[.] is not infinite, please use [..] or [...] instead.");
            throw StyioSyntaxError(errmsg);
          };

          return parse_loop(tok_ctx, cur_char);
        }
        else
        {
          return parse_list_expr(tok_ctx, cur_char);
        }
      }
      
      // You should NOT reach this line!
      break;

    case '{':
      cur_char = get_next_char();
      
      // "{" |--

      break;

    case '>':
      {
        // eliminate >
        cur_char = get_next_char();
        
        if (check_this_char(cur_char, '_')) {
          // eliminate _
          cur_char = get_next_char();

          drop_white_spaces(cur_char);

          if (check_this_char(cur_char, '('))
          {
            // eliminate (
            cur_char = get_next_char();

            drop_all_spaces(cur_char);

            if (check_this_char(cur_char, '\"'))
            {
              return new WriteStdOutAST(parse_string(tok_ctx, cur_char));
            }
            else
            {
              std::string errmsg = std::string("Cannot parse the thing to be printed, starts with ") + char(cur_char) + ".";
              throw StyioSyntaxError(errmsg);
            };
          }
          else
          if (check_this_char(cur_char, '\"'))
          {
            return new WriteStdOutAST(parse_string(tok_ctx, cur_char));
          }
          else
          {
            std::string errmsg = std::string("Try >_(\"Styio\").");
            throw StyioSyntaxError(errmsg);
          };

          StringAST* strAST = parse_string(tok_ctx, cur_char);

          WriteStdOutAST* result = new WriteStdOutAST(strAST);

          return result;
        };
      }

      // You should NOT reach this line!
      break;
      
    default:
      break;
  }

  std::string errmsg = std::string("Unrecognized statement, starting with `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

std::string parse_ext_elem(std::vector<int>& tok_ctx, int& cur_char)
{
  std::string itemStr;

  if (check_this_char(cur_char, '\"'))
  {
    // eliminate double quote symbol " at the start of dependency item
    cur_char = get_next_char();

    while (cur_char != '\"') 
    {
      if (check_this_char(cur_char, ',')) 
      {
        std::string errmsg = std::string("An \" was expected after") + itemStr + "however, a delimeter `,` was detected. ";
        throw StyioSyntaxError(errmsg);
      }

      itemStr += cur_char;

      cur_char = get_next_char();
    };

    // eliminate double quote symbol " at the end of dependency item
    cur_char = get_next_char();

    return itemStr;
  }
  else
  {
    std::string errmsg = std::string("Dependencies should be wrapped with double quote like \"abc/xyz\", rather than starting with the character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

ExtPackAST* parse_ext_pack (std::vector<int>& tok_ctx, int& cur_char) 
{ 
  // eliminate left square (box) bracket [
  cur_char = get_next_char();

  std::vector<std::string> dependencies;

  drop_all_spaces(cur_char);

  // add the first dependency path to the list
  dependencies.push_back(parse_ext_elem(tok_ctx, cur_char));

  std::string pathStr = "";
  
  while (check_this_char(cur_char, ',')) {
    // eliminate comma ","
    cur_char = get_next_char();

    // reset pathStr to empty ""
    pathStr = ""; 

    drop_all_spaces(cur_char);
    
    // add the next dependency path to the list
    dependencies.push_back(parse_ext_elem(tok_ctx, cur_char));
  };

  if (check_this_char(cur_char, ']')) {
    // eliminate right square bracket `]` after dependency list
    cur_char = get_next_char();
  };

  ExtPackAST* result = new ExtPackAST(dependencies);

  return result;
}

StyioAST* parse_block (std::vector<int>& tok_ctx, int& cur_char) 
{
  // the last expression will be the return expression
  // a block must have a return value
  // either an expression
  // or null

  std::vector<StyioAST*> stmtBuffer;

  // eliminate { at the start
  get_next_char(cur_char);

  while (1)
  {
    drop_all_spaces(cur_char);
    
    if (check_this_char(cur_char, '}'))
    {
      // eliminate } at the end
      get_next_char(cur_char);

      break;
    }
    else
    {
      StyioAST* tmpStmt = parse_stmt(tok_ctx, cur_char);
      stmtBuffer.push_back(tmpStmt);
    };
  };

  if (stmtBuffer.size() == 0)
  {
    return new EmptyBlockAST();
  }
  else
  {
    return new BlockAST(stmtBuffer);
  };
}

void parse_program () 
{
  std::vector<int> tok_ctx;
  static int cur_char = ' ';

  while (1) 
  {
    fprintf(stderr, "<Styio/> ");

    cur_char = get_next_char();

    StyioAST* stmt = parse_stmt(tok_ctx, cur_char);

    std::cout << stmt -> toString() << std::endl;

    // std::cout << "Next Line >>" << std::endl;
  }; 
}