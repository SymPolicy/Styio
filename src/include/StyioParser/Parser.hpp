#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

template <typename Enumeration>
auto type_to_int(Enumeration const value)
    -> typename std::underlying_type<Enumeration>::type
{
    return static_cast<typename std::underlying_type<Enumeration>::type>(value);
}

static int get_next_char() 
{
  return getchar();
}

static void drop_all_spaces (int& cur_char) 
{
  while (isspace(cur_char)) {
    cur_char = get_next_char();
  };
}


static void drop_white_spaces (int& cur_char) 
{
  while (cur_char == ' ') {
    cur_char = get_next_char();
  };
}

static bool is_bin_tok (int& cur_char) 
{
  switch (cur_char)
  {
  case '+':
    return true;

  case '-':
    return true;

  case '*':
    return true;

  case '/':
    return true;

  case '%':
    return true;
  
  default:
    return false;
  }
}

static IdAST* parse_id (std::vector<int>& tok_ctx, int& cur_char) 
{
  std::string idStr = "";
  idStr += cur_char;

  // [a-zA-Z][a-zA-Z0-9_]*
  while (isalnum((cur_char = get_next_char())) || cur_char == '_') 
  {
    idStr += cur_char;
  }

  return new IdAST(idStr);
}

static IntAST* parse_int (std::vector<int>& tok_ctx, int& cur_char)
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

static StyioAST* parse_int_or_float (std::vector<int>& tok_ctx, int& cur_char)
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

  if (cur_char == '.') 
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

static StringAST* parse_string (std::vector<int>& tok_ctx, int& cur_char) 
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

static SizeOfAST* parse_size_of (std::vector<int>& tok_ctx, int& cur_char) 
{
  cur_char = get_next_char();
       
  if (isalpha(cur_char) || cur_char == '_')
  {
    IdAST* var = parse_id(tok_ctx, cur_char);

    if (cur_char == '|') {
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

static StyioAST* parse_ext_res (std::vector<int>& tok_ctx, int& cur_char) 
{
  // eliminate @
  cur_char = get_next_char();

  if (cur_char == '(') {
    // eliminate (
    cur_char = get_next_char();

    if (cur_char == '\"') {
      // eliminate the left double quote "
      cur_char = get_next_char();

      std::string textStr = "";
  
      while (cur_char != '\"')
      {
        textStr += cur_char;
        cur_char = get_next_char();
      };

      if (cur_char == '\"') {
        // eliminate the right double quote "
        cur_char = get_next_char();
      }
      else
      {
        std::string errmsg = std::string("Expecting \" at the end, but got `") + char(cur_char) + "`.";
        throw StyioSyntaxError(errmsg);
      };

      if (cur_char == ')') {
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

static StyioAST* parse_list_elem (std::vector<int>& tok_ctx, int& cur_char) 
{
  if (isdigit(cur_char)) 
  {
    return parse_int_or_float(tok_ctx, cur_char);
  }
  else if (isalpha(cur_char) || cur_char == '_') 
  {
    return parse_id(tok_ctx, cur_char);
  }
  else if (cur_char == '\"') 
  {
    return parse_string(tok_ctx, cur_char);
  }
  
  std::string errmsg = std::string("Unexpected List / Range Element, starts with character `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

static ListOpAST* parse_list_op (
  std::vector<int>& tok_ctx, 
  int& cur_char,
  StyioAST* theList
) 
{
  // eliminate [ at the start
  cur_char = get_next_char();

  switch (cur_char)
  {
  // List.get_reversed()
  case '<':
    return new ListOpAST(
      theList, 
      ListOpType::Reversed);

    break;

  // List.get_item_by_list(item)
  case '?':
    {
      cur_char = get_next_char();

      if (cur_char == '=')
      {
        cur_char = get_next_char();

        StyioAST* theItem = parse_list_elem(tok_ctx, cur_char);

        return new ListOpAST(
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
    break;
  
  // List.insert_item_by_index(index, item)
  case '^':
    cur_char = get_next_char();

    // eliminate white spaces between ^ and index
    drop_white_spaces(cur_char);

    StyioAST* theIndex = parse_int(tok_ctx, cur_char);

    // eliminate white spaces between index and <-
    drop_white_spaces(cur_char);

    if (cur_char == '<')
    {
      cur_char = get_next_char();

      if (cur_char == '-')
      {
        cur_char = get_next_char();

        // eliminate white spaces between <- and the value to be inserted
        drop_white_spaces(cur_char);

        // the item to be inserted into the list
        StyioAST* theItemIns = parse_list_elem(tok_ctx, cur_char);

        return new ListOpAST(
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

    break;
  
  default:
    break;
  }

  // check [ at the end
  if (cur_char == ']')
  {
    // eliminate [ at the end
    cur_char = get_next_char();
    
  }
  else
  {
    std::string errmsg = std::string("Missing `]` after List[Operation].");
    throw StyioSyntaxError(errmsg);
  };
}

static StyioAST* parse_list_expr (std::vector<int>& tok_ctx, int& cur_char) 
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

      while (cur_char == '.')
      {
        cur_char = get_next_char();
      }
      
      StyioAST* endEl = parse_list_elem(tok_ctx, cur_char);

      StyioAST* list_loop;

      if (startEl -> hint() == StyioType::Int 
        && endEl -> hint() == StyioType::Id)
      {
        list_loop = new InfLoop(startEl, endEl);
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

      if (cur_char == ']') 
      {
        cur_char = get_next_char();
      }
      else
      {
        std::string errmsg = std::string("Missing `]` after List / Range / Loop: `") + char(cur_char) + "`";
        throw StyioSyntaxError(errmsg);
      };

      switch (cur_char)
      {
      case '\n':
        {
          // If: LF, Then: Statement Ends
          cur_char = get_next_char();

          return list_loop;
        }
        
        // You should NOT reach this line!
        break;

      case '>':
        {
          cur_char = get_next_char();

          if (cur_char == '>')
          {
            // If: >>, Then: Iteration
            cur_char = get_next_char();
            
          }
          
          cur_char = get_next_char();

          // TODO: Iteration Over List / Range / Loop

          return list_loop;
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
      while (cur_char == ',')
      {
        // eliminate ,
        cur_char = get_next_char();

        drop_white_spaces(cur_char);

        if (cur_char == ']') 
        {
          cur_char = get_next_char();

          return new ListAST(elements);
        };

        StyioAST* el = parse_list_elem(tok_ctx, cur_char);

        elements.push_back(el);
      };

      drop_white_spaces(cur_char);

      if (cur_char == ']') 
      {
        cur_char = get_next_char();

        return new ListAST(elements);
      }
      else
      {
        std::string errmsg = std::string("Missing `]` after List / Range / Loop: `") + char(cur_char) + "`";
        throw StyioSyntaxError(errmsg);
      };
    }

    // You should not reach this line!
    break;
  
  default:
    break;
  }

  std::string errmsg = std::string("Uncompleted List, ends with character `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

static InfLoop* parse_loop (std::vector<int>& tok_ctx, int& cur_char)
{
  while (cur_char == '.') 
  { 
    // eliminate all .
    cur_char = get_next_char();

    if (cur_char == ']') 
    {
      cur_char = get_next_char();

      return new InfLoop();
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


static StyioAST* parse_bin_rhs (
  std::vector<int>& tok_ctx, 
  int& cur_char
) 
{
  drop_white_spaces(cur_char);

  // ID
  if (isalpha(cur_char) || cur_char == '_') {
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

        if (cur_char == ']') {
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

static BinOpAST* parse_bin_op (
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
        if (cur_char == '*')
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

static StyioAST* parse_value_expr (
  std::vector<int>& tok_ctx, 
  int& cur_char
)
{
  // <ID>
  if (isalpha(cur_char) || cur_char == '_') 
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

        if (cur_char == ']') {
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

static MutAssignAST* parse_mut_assign (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  IdAST* id_ast
)
{
  MutAssignAST* output = new MutAssignAST(id_ast, parse_value_expr(tok_ctx, cur_char));
  
  if (cur_char == '\n') 
  {
    return output;
  }
  else
  {
    std::string errmsg = std::string("Unexpected character `") + char(cur_char) + "` after Assign(Mutable)";
    throw StyioSyntaxError(errmsg);
  }
}

static FixAssignAST* parse_fix_assign (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  IdAST* id_ast
) 
{
  FixAssignAST* output = new FixAssignAST(id_ast, parse_value_expr(tok_ctx, cur_char));
  
  if (cur_char == '\n') 
  {
    return output;
  }
  else
  {
    std::string errmsg = std::string("Unexpected character `") + char(cur_char) + "` after Assign(Mutable)";
    throw StyioSyntaxError(errmsg);
  }
}

static StyioAST* parse_read_file (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  IdAST* id_ast
) 
{
  if (cur_char == '@')
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

static StyioAST* parse_stmt (std::vector<int>& tok_ctx, int& cur_char) 
{
  while (cur_char != '\n')
  {
    drop_white_spaces(cur_char);

    // <ID>
    if (isalpha(cur_char) || cur_char == '_') 
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
            if (cur_char == '=')
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

            if (cur_char == '-')
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

          if (cur_char == '(') 
          {
            cur_char = get_next_char();

          };
          
          std::vector<IdAST*> varBuffer;

          if (isalpha(cur_char) || cur_char == '_') {
            // "@" "(" |--
            IdAST* id_ast = parse_id(tok_ctx, cur_char);
        
            varBuffer.push_back(id_ast);
          };

          drop_white_spaces(cur_char);

          // "@" "(" [<ID> |--
          while (cur_char == ',')
          {
            cur_char = get_next_char();

            drop_white_spaces(cur_char);

            if (isalpha(cur_char) || cur_char == '_') {
              
              IdAST* id_ast = parse_id(tok_ctx, cur_char);
        
              varBuffer.push_back(id_ast);

              drop_white_spaces(cur_char);
            };
          };
          
          if (cur_char == ')') 
          {
            cur_char = get_next_char();
          };

          VarDefAST* varDef = new VarDefAST(varBuffer);

          // if (cur_char == '[') 
          // {
          //   // eliminate single [
          //   cur_char = get_next_char();

          //   if (isdigit(cur_char)) 
          //   {
          //     IntAST* startInt = parseInt(tok_ctx, cur_char);
          //   };

          //   int dotCount = 0;

          //   while (cur_char == '.')
          //   {
          //     // eliminate all .
          //     cur_char = get_next_char();

          //     dotCount += 1;
          //   };
            
          //   if (isdigit(cur_char)) 
          //   {
          //     IntAST* endInt = parseInt(tok_ctx, cur_char);
          //   };

          //   if (cur_char == ']')
          //   {
          //     // eliminate single ]
          //     cur_char = get_next_char();
          //   };

          //   while (cur_char == '>')
          //   {
          //     // eliminate all >
          //     cur_char = get_next_char();
          //   }

          //   BlockAST* block = parseBlock(tok_ctx, cur_char);

          //   LoopAST* loop = new LoopAST(startInt, IntAST(1), block);
          // }

          return varDef;
          
          // You should NOT reach this line!
          break;
        };

      case ':':
        cur_char = get_next_char();

        if (cur_char == '=') {
          cur_char = get_next_char();
        } 
        else
        {

        };
        
        break;

      case '-':
        cur_char = get_next_char();

        if (cur_char == '>') {
          cur_char = get_next_char();
        };
        
        break;

      case '?':
        cur_char = get_next_char();
        
        if (cur_char == '=') {
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
          
          if (cur_char == '~') {
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

          if (cur_char == '.') {
            // eliminate the first dot .
            cur_char = get_next_char();

            if (cur_char == ']') 
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
          
          if (cur_char == '_') {
            // eliminate _
            cur_char = get_next_char();

            drop_white_spaces(cur_char);

            if (cur_char == '(')
            {
              // eliminate (
              cur_char = get_next_char();

              drop_all_spaces(cur_char);

              if (cur_char == '\"')
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
            if (cur_char == '\"')
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

    std::cout << "Unknown: " << char(cur_char) << std::endl;
  };

  return new NoneAST();
}

static std::string parse_ext_elem(std::vector<int>& tok_ctx, int& cur_char)
{
  std::string itemStr;

  if (cur_char == '\"')
  {
    // eliminate double quote symbol " at the start of dependency item
    cur_char = get_next_char();

    while (cur_char != '\"') 
    {
      if (cur_char == ',') 
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

/*
parse_ext_pack

Dependencies should be written like a list of paths
like this -> ["ab/c", "x/yz"]

// 1. The dependencies should be parsed before any domain (statement/expression). 
// 2. The left square bracket `[` is only eliminated after entering this function (parse_ext_pack)
|-- "[" <PATH>+ "]"

If ? ( "the program starts with a left square bracket `[`" ),
then -> { 
  "parse_ext_pack() starts";
  "eliminate the left square bracket `[`";
  "parse dependency paths, which take comma `,` as delimeter";
  "eliminate the right square bracket `]`";
} 
else :  { 
  "parse_ext_pack() should NOT be invoked in this case";
  "if starts with left curly brace `{`, try parseSpace()";
  "otherwise, try parseScript()";
}

*/
static ExtPackAST* parse_ext_pack (std::vector<int>& tok_ctx, int& cur_char) 
{ 
  // eliminate left square (box) bracket [
  cur_char = get_next_char();

  std::vector<std::string> dependencies;

  drop_all_spaces(cur_char);

  // add the first dependency path to the list
  dependencies.push_back(parse_ext_elem(tok_ctx, cur_char));

  std::string pathStr = "";
  
  while (cur_char == ',') {
    // eliminate comma ","
    cur_char = get_next_char();

    // reset pathStr to empty ""
    pathStr = ""; 

    drop_all_spaces(cur_char);
    
    // add the next dependency path to the list
    dependencies.push_back(parse_ext_elem(tok_ctx, cur_char));
  };

  if (cur_char == ']') {
    // eliminate right square bracket `]` after dependency list
    cur_char = get_next_char();
  };

  ExtPackAST* result = new ExtPackAST(dependencies);

  return result;
}

static BlockAST* parseBlock (std::vector<int>& tok_ctx, int& cur_char) 
{
  // the last expression will be the return expression
  // a block must have a return value
  // either an expression
  // or null

  std::vector<StyioAST*> stmtBuffer;

  StyioAST* exprAST = parse_stmt(tok_ctx, cur_char);
  
  if (cur_char == ';')
  {
    stmtBuffer.push_back(exprAST);

    while (cur_char == ';')
    {
      // eliminate ;
      cur_char = get_next_char();

      StyioAST* exprAST = parse_stmt(tok_ctx, cur_char);
      stmtBuffer.push_back(exprAST);
    }

    BlockAST* result = new BlockAST(stmtBuffer, exprAST);

    return result;
  }
  else
  {
    BlockAST* result = new BlockAST(exprAST);

    return result;
  };
}

static void parseSpace (std::vector<int>& tok_ctx, int& cur_char) 
{
  
}

static void parseScript (std::vector<int>& tok_ctx, int& cur_char) {
  if (cur_char == '{') 
  {
    // eliminate "{"
    cur_char = get_next_char();

    parseBlock(tok_ctx, cur_char);

    // eliminate "}"
    if (cur_char == '}')
    {
      cur_char = get_next_char();
    }
    else
    {
      std::string errmsg = std::string("Missing `}` after code block.");
      throw StyioSyntaxError(errmsg);
    };
  }
  else
  {
    parseBlock(tok_ctx, cur_char);
  };
}

static std::vector<int> parse_program () 
{
  std::vector<int> tok_ctx;
  static int cur_char = ' ';

  while (1) 
  {
    fprintf(stderr, "<Styio/> ");

    cur_char = get_next_char();

    StyioAST* stmt = parse_stmt(tok_ctx, cur_char);

    std::cout << stmt -> toString() << std::endl;

    std::cout << "Next Line >>" << std::endl;
  }; 

  return tok_ctx;
}

#endif