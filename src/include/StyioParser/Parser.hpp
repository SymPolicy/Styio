#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

static int get_next_char() 
{
  int tmpChar = getchar();

  return tmpChar;
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
  intStr += cur_char;
  cur_char = get_next_char();

  // [0-9]*
  while (isdigit(cur_char))
  {
    intStr += cur_char;
    cur_char = get_next_char();
  };

  return new IntAST(std::stoi(intStr));
}

static StyioAST* parse_int_float (std::vector<int>& tok_ctx, int& cur_char)
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
    numStr += cur_char;
    cur_char = get_next_char();

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

  tok_ctx.push_back(
    StyioToken::TOK_STRING
  );

  return new StringAST(textStr);
}

static StyioAST* parseExtRes (std::vector<int>& tok_ctx, int& cur_char) 
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

      return new PathAST(textStr);
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
    return parse_int_float(tok_ctx, cur_char);
  }
  else if (isalpha(cur_char) || cur_char == '_') 
  {
    return parse_id(tok_ctx, cur_char);
  }
  else if (cur_char == '\"') 
  {
    return parse_string(tok_ctx, cur_char);
  }
  
  std::string errmsg = std::string("Unexpected Element for Iterator, starts with character `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

static ListAST* parse_list_expr (std::vector<int>& tok_ctx, int& cur_char) 
{
  std::vector<StyioAST*> elements;

  StyioAST* el = parse_list_elem(tok_ctx, cur_char);
  elements.push_back(el);

  drop_white_spaces(cur_char);

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
  };

  std::string errmsg = std::string("Uncompleted List, ends with character `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

static InfiniteAST* parse_loop (std::vector<int>& tok_ctx, int& cur_char)
{
  while (cur_char == '.') 
  { 
    // eliminate all .
    cur_char = get_next_char();

    if (cur_char == ']') 
    {
      cur_char = get_next_char();

      return new InfiniteAST();
    };
  };

  if (isdigit(cur_char))
  {
    StyioAST* errnum = parse_int_float(tok_ctx, cur_char);
    
    std::string errmsg = std::string("A finite list must have both start and end values. However, only the end value is detected: `") + errnum -> toStringInline() + "`. Try `[0.." + errnum -> toStringInline() + "]` rather than `[.." + errnum -> toStringInline() + "]`.";
    throw StyioSyntaxError(errmsg);
  }

  std::string errmsg = std::string("Unexpected character `") + char(cur_char) + "` in infinite expression.";
  throw StyioSyntaxError(errmsg);
}


static StyioAST* parseBinRHS (
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
    StyioAST* result = parse_int_float(tok_ctx, cur_char);
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
          ListAST* result = parse_list_expr(tok_ctx, cur_char);
          return result;
        }
      }

      // You should NOT reach this line.
      break;

    // SizeOf()
    case '|':
      {
        cur_char = get_next_char();
       
        if (isalpha(cur_char) || cur_char == '_')
        {
          IdAST* var = parse_id(tok_ctx, cur_char);
          SizeOfAST* result = new SizeOfAST(var);
          return result;
        }
        else
        {
          std::string errmsg = std::string("Unexpected SizeOf(), starts with `") + char(cur_char) + "`";
          throw StyioSyntaxError(errmsg);
        }

        if (cur_char == '|') {
          cur_char = get_next_char();
        }
        else
        {
          std::string errmsg = std::string("Expecting | at the end of SizeOf(), but got `") + char(cur_char) + "`";
          throw StyioSyntaxError(errmsg);
        }
      }

      // You should NOT reach this line.
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

          // <ID> "+" | -> 
          binOp = new BinOpAST(BinTok::BIN_ADD, lhs_ast, parseBinRHS(tok_ctx, cur_char));
        };

        // You should NOT reach this line.
        break;

      // BIN_SUB := <ID> "-" <EXPR>
      case '-':
        {
          cur_char = get_next_char();

          // <ID> "-" | ->
          binOp = new BinOpAST(BinTok::BIN_SUB, lhs_ast, parseBinRHS(tok_ctx, cur_char));
        };

        // You should NOT reach this line.
        break;

      // BIN_MUL | BIN_POW
      case '*':
        {
          cur_char = get_next_char();
          // BIN_POW := <ID> "**" <EXPR>
          if (cur_char == '*')
          {
            cur_char = get_next_char();

            // <ID> "**" | ->
            binOp = new BinOpAST(BinTok::BIN_POW, lhs_ast, parseBinRHS(tok_ctx, cur_char));
          } 
          // BIN_MUL := <ID> "*" <EXPR>
          else 
          {
            // <ID> "*" | ->
            binOp = new BinOpAST(BinTok::BIN_MUL, lhs_ast, parseBinRHS(tok_ctx, cur_char));
          }
        };
        // You should NOT reach this line.
        break;
        
      // BIN_DIV := <ID> "/" <EXPR>
      case '/':
        {
          cur_char = get_next_char();

          // <ID> "/" | -> 
          binOp = new BinOpAST(BinTok::BIN_DIV, lhs_ast, parseBinRHS(tok_ctx, cur_char));
        };

        // You should NOT reach this line.
        break;

      // BIN_MOD := <ID> "%" <EXPR> 
      case '%':
        {
          cur_char = get_next_char();

          // <ID> "%" | -> 
          binOp = new BinOpAST(BinTok::BIN_MOD, lhs_ast, parseBinRHS(tok_ctx, cur_char));
        };

        // You should NOT reach this line.
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

static StyioAST* parse_assign_value (
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

    // check next character
    switch (cur_char)
    {
      // BIN_ADD := <ID> "+" <EXPR>
      case '+':
        {
          // <ID> | -> 
          return parse_bin_op(tok_ctx, cur_char, id_ast);
        };

        // You should NOT reach this line.
        break;

      // BIN_SUB := <ID> "-" <EXPR>
      case '-':
        {
          // <ID> | ->
          return parse_bin_op(tok_ctx, cur_char, id_ast);
        };

        // You should NOT reach this line.
        break;

      // BIN_MUL | BIN_POW
      case '*':
        {
          // <ID> | ->
          return parse_bin_op(tok_ctx, cur_char, id_ast);
        };
        // You should NOT reach this line.
        break;
        
      // BIN_DIV := <ID> "/" <EXPR>
      case '/':
        {
          // <ID> "/" | -> 
          return parse_bin_op(tok_ctx, cur_char, id_ast);
        };

        // You should NOT reach this line.
        break;

      // BIN_MOD := <ID> "%" <EXPR> 
      case '%':
        {
          // <ID> | -> 
          return parse_bin_op(tok_ctx, cur_char, id_ast);
        };

        // You should NOT reach this line.
        break;
      
      default:
        return id_ast;

        // You should NOT reach this line.
        break;
    }
  }
  else
  if (isdigit(cur_char)) {
    StyioAST* numAST = parse_int_float(tok_ctx, cur_char);

    // ignore white spaces after number
    drop_white_spaces(cur_char);

    switch (cur_char)
    {
      // BIN_ADD := [<Int>|<Float>] "+" <EXPR>
      case '+':
        {
          // [<Int>|<Float>] | ->
          return parse_bin_op(tok_ctx, cur_char, numAST);
        };

        // You should NOT reach this line.
        break;

      // BIN_SUB := [<Int>|<Float>] "-" <EXPR>
      case '-':
        {
          // [<Int>|<Float>] | ->
          return parse_bin_op(tok_ctx, cur_char, numAST);
        };

        // You should NOT reach this line.
        break;

      // BIN_MUL | BIN_POW
      case '*':
        {
          // [<Int>|<Float>] | ->
          return parse_bin_op(tok_ctx, cur_char, numAST);
        }

        // You should NOT reach this line.
        break;

      // BIN_DIV := [<Int>|<Float>] "/" <EXPR>
      case '/':
        {
          // [<Int>|<Float>] | ->
          return parse_bin_op(tok_ctx, cur_char, numAST);
        };

        // You should NOT reach this line.
        break;

      // BIN_MOD := [<Int>|<Float>] "%" <EXPR>
      case '%':
        {
          // [<Int>|<Float>] | ->
          return parse_bin_op(tok_ctx, cur_char, numAST);
        };

        // You should NOT reach this line.
        break;

      default:
        return numAST;

        // You should NOT reach this line.
        break;
    }
  }
  else
  {
    switch (cur_char)
    {
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

      // You should NOT reach this line.
      break;

    case '|':
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

      // You should NOT reach this line.
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
  MutAssignAST* output = new MutAssignAST(id_ast, parse_assign_value(tok_ctx, cur_char));
  
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
  FixAssignAST* output = new FixAssignAST(id_ast, parse_assign_value(tok_ctx, cur_char));
  
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
    StyioAST* value = parseExtRes(tok_ctx, cur_char);

    return new ReadAST(id_ast, value);
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
            tok_ctx.push_back(
              StyioToken::TOK_LF
            );
            return id_ast;
          };

          // You should NOT reach this line.
          break;

        // <ID> = <EXPR>
        case '=':
          {
            cur_char = get_next_char();

            drop_white_spaces(cur_char);

            // <ID> = | ->
            return parse_mut_assign(tok_ctx, cur_char, id_ast);
          };

          // You should NOT reach this line.
          break;
        
        // <ID> := <EXPR>
        case ':':
          {
            cur_char = get_next_char();
            if (cur_char == '=')
            {
              cur_char = get_next_char();

              tok_ctx.push_back(
                StyioToken::TOK_WALRUS
              );

              drop_white_spaces(cur_char);
              
              // <ID> := | ->
              return parse_fix_assign(tok_ctx, cur_char, id_ast);
            }
            else
            {
              std::string errmsg = std::string("Unexpected `:`");
              throw StyioSyntaxError(errmsg);
            }
          };

          // You should NOT reach this line.
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
              
              // <ID> <- | ->
              return parse_read_file(tok_ctx, cur_char, id_ast);
            }
            else
            {
              std::string errmsg = std::string("Expecting `-` after `<`, but found `") + char(cur_char) + "`.";
              throw StyioSyntaxError(errmsg);
            }
          };

          // You should NOT reach this line.
          break;

        // BIN_ADD := <ID> "+" <EXPR>
        case '+':
          {
            // <ID> | -> 
            return parse_bin_op(tok_ctx, cur_char, id_ast);
          };

          // You should NOT reach this line.
          break;

        // BIN_SUB := <ID> "-" <EXPR>
        case '-':
          {
            // <ID> | ->
            return parse_bin_op(tok_ctx, cur_char, id_ast);
          };

          // You should NOT reach this line.
          break;

        // BIN_MUL | BIN_POW
        case '*':
          {
            // <ID> | ->
            return parse_bin_op(tok_ctx, cur_char, id_ast);
          };
          // You should NOT reach this line.
          break;
          
        // BIN_DIV := <ID> "/" <EXPR>
        case '/':
          {
            // <ID> "/" | -> 
            return parse_bin_op(tok_ctx, cur_char, id_ast);
          };

          // You should NOT reach this line.
          break;

        // BIN_MOD := <ID> "%" <EXPR> 
        case '%':
          {
            // <ID> | -> 
            return parse_bin_op(tok_ctx, cur_char, id_ast);
          };

          // You should NOT reach this line.
          break;
        
        default:
          break;
      }
    }

    if (isdigit(cur_char)) {
      StyioAST* numAST = parse_int_float(tok_ctx, cur_char);

      drop_all_spaces(cur_char);

      switch (cur_char)
      {
        // <LF>
        case '\n':
          {
            // simply eliminate LF
            cur_char = get_next_char();
            tok_ctx.push_back(
              StyioToken::TOK_LF
            );
            return numAST;
          };

          // You should NOT reach this line.
          break;

        // BIN_ADD := [<Int>|<Float>] "+" <EXPR>
        case '+':
          {
            // [<Int>|<Float>] | ->
            BinOpAST* bin_ast = parse_bin_op(tok_ctx, cur_char, numAST);
            return bin_ast;
          };

          // You should NOT reach this line.
          break;

        // BIN_SUB := [<Int>|<Float>] "-" <EXPR>
        case '-':
          {
            // [<Int>|<Float>] | ->
            BinOpAST* bin_ast = parse_bin_op(tok_ctx, cur_char, numAST);
            return bin_ast;
          };

          // You should NOT reach this line.
          break;

        // BIN_MUL | BIN_POW
        case '*':
          {
            // [<Int>|<Float>] | ->
            BinOpAST* bin_ast = parse_bin_op(tok_ctx, cur_char, numAST);
            return bin_ast;
          }

          // You should NOT reach this line.
          break;

        // BIN_DIV := [<Int>|<Float>] "/" <EXPR>
        case '/':
          {
            // [<Int>|<Float>] | ->
            BinOpAST* bin_ast = parse_bin_op(tok_ctx, cur_char, numAST);
            return bin_ast;
          };

          // You should NOT reach this line.
          break;

        // BIN_MOD := [<Int>|<Float>] "%" <EXPR>
        case '%':
          {
            // [<Int>|<Float>] | ->
            BinOpAST* bin_ast = parse_bin_op(tok_ctx, cur_char, numAST);
            return bin_ast;
          };

          // You should NOT reach this line.
          break;

        default:
          return numAST;

          // You should NOT reach this line.
          break;
      }
    }

    switch (cur_char)
    {
      // VAR_DEF := "@" "(" [<ID> ["," <ID>]*]? ")"
      case '@':
        {
          cur_char = get_next_char();

          tok_ctx.push_back(
            StyioToken::TOK_AT
          );
          
          drop_white_spaces(cur_char);

          if (cur_char == '(') 
          {
            cur_char = get_next_char();

            tok_ctx.push_back(
              StyioToken::TOK_LPAREN
            );
          };
          
          std::vector<IdAST*> varBuffer;

          if (isalpha(cur_char) || cur_char == '_') {
            // "@" "(" | ->
            IdAST* id_ast = parse_id(tok_ctx, cur_char);
        
            varBuffer.push_back(id_ast);
          };

          drop_white_spaces(cur_char);

          // "@" "(" [<ID> | ->
          while (cur_char == ',')
          {
            cur_char = get_next_char();

            drop_white_spaces(cur_char);

            if (isalpha(cur_char) || cur_char == '_') {
              tok_ctx.push_back(
                StyioToken::TOK_COMMA
              );
              
              IdAST* id_ast = parse_id(tok_ctx, cur_char);
        
              varBuffer.push_back(id_ast);

              drop_white_spaces(cur_char);
            };
          };
          
          if (cur_char == ')') 
          {
            cur_char = get_next_char();

            tok_ctx.push_back(
              StyioToken::TOK_RPAREN
            );
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
          
          // You should NOT reach this line.
          break;
        };

      case ':':
        cur_char = get_next_char();

        if (cur_char == '=') {
          cur_char = get_next_char();

          tok_ctx.push_back(
            StyioToken::TOK_WALRUS
          );
        } 
        else
        {
          tok_ctx.push_back(
            StyioToken::TOK_COLON
          );
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

          tok_ctx.push_back(
            StyioToken::TOK_MATCH
          );
        } 
        else 
        {
          tok_ctx.push_back(
            StyioToken::TOK_CHECK
          );
        };
        
        break;
      
      case '\"':
        parse_string(tok_ctx, cur_char);
        break;

      case '!':
        {
          cur_char = get_next_char();
          tok_ctx.push_back(
            StyioToken::TOK_EXCLAM
          );
          
          if (cur_char == '~') {
            cur_char = get_next_char();
            tok_ctx.push_back(
              StyioToken::TOK_TILDE
            );
          };
        };

        // You should NOT reach this line.
        break;

      case ',':
        tok_ctx.push_back(
          StyioToken::TOK_COMMA
        );
        cur_char = get_next_char();
        break;

      case '.':
        tok_ctx.push_back(
          StyioToken::TOK_DOT
        );
        cur_char = get_next_char();
        break;

      case ';':
        tok_ctx.push_back(
          StyioToken::TOK_SEMICOLON
        );
        cur_char = get_next_char();
        break;

      case '(':
        tok_ctx.push_back(
          StyioToken::TOK_LPAREN
        );
        cur_char = get_next_char();
        break;

      case ')':
        tok_ctx.push_back(
          StyioToken::TOK_RPAREN
        );
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

            InfiniteAST* result = parse_loop(tok_ctx, cur_char);
            return result;
          }
          else
          {
            StyioAST* result = parse_list_expr(tok_ctx, cur_char);
            return result;
          }
        }
        
        // You should NOT reach this line.
        break;

      case ']':
        tok_ctx.push_back(
          StyioToken::TOK_RBOXBRAC
        );
        cur_char = get_next_char();
        
        break;

      case '{':
        cur_char = get_next_char();
        
        tok_ctx.push_back(
          StyioToken::TOK_LCURBRAC
        );
        
        // "{" | ->

        break;

      case '}':
        cur_char = get_next_char();
        
        tok_ctx.push_back(
          StyioToken::TOK_RCURBRAC
        );

        break;

      case '<':
        cur_char = get_next_char();
        tok_ctx.push_back(
          StyioToken::TOK_LANGBRAC
        );
        break;

      case '>':
        {
          // eliminate >
          cur_char = get_next_char();
          
          if (cur_char == '_') {
            // eliminate _
            cur_char = get_next_char();
            tok_ctx.push_back(
              StyioToken::TOK_STDOUT
            );

            drop_white_spaces(cur_char);

            StringAST* strAST = parse_string(tok_ctx, cur_char);

            StdOutAST* result = new StdOutAST(strAST);

            return result;
          };
        }

        // You should NOT reach this line.
        break;
        
      default:
        break;
    }

    std::cout << "Next: " << char(cur_char) << std::endl;
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
| -> "[" <PATH>+ "]"

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
static ExtPacAST* parse_ext_pack (std::vector<int>& tok_ctx, int& cur_char) 
{ 
  // eliminate left square (box) bracket [
  cur_char = get_next_char();
  tok_ctx.push_back(
    StyioToken::TOK_LBOXBRAC
  );

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

  ExtPacAST* result = new ExtPacAST(dependencies);

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
    fprintf(stderr, "</> ");

    cur_char = get_next_char();

    StyioAST* stmt = parse_stmt(tok_ctx, cur_char);

    std::cout << stmt -> toString() << std::endl;
  }; 

  return tok_ctx;
}

#endif