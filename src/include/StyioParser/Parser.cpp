#include <string>
#include <tuple>
#include <vector>
#include <fstream>
#include <iostream>

#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"
#include "../StyioUtil/Util.hpp"
#include "Parser.hpp"

/*
  1. Let the context decide the parsing process.
*/

/*
  =================
*/

void get_next_char(
  struct StyioCodeContext* code,
  int& cur_char
)
{
  // std::cout << "This: " << code -> text.at(code -> cursor) << " | (" << code -> cursor << ")" << std::endl;

  code -> cursor += 1;
  cur_char = code -> text.at(code -> cursor);

  // std::cout << "Next: " << char(cur_char) << " | (" << code -> cursor << ") \n ---------------" << std::endl;
}

bool check_this_char(
  int& cur_char, 
  char value
)
{
  return cur_char == value;
}

void drop_all_spaces (
  struct StyioCodeContext* code,
  int& cur_char
) 
{
  while (isspace(cur_char)) {
    get_next_char(code, cur_char);
  };
}

void drop_white_spaces (
  struct StyioCodeContext* code,
  int& cur_char
) 
{
  while (check_this_char(cur_char, ' ')) {
    get_next_char(code, cur_char);
  };
}

void check_and_drop (
  struct StyioCodeContext* code,
  int& cur_char,
  char value,
  int mode = 0)
{
  switch (mode)
  {
  case 1:
    drop_white_spaces(code, cur_char);

    break;

  case 2:
    drop_all_spaces(code, cur_char);

    break;
  
  default:
    break;
  }

  if (check_this_char(cur_char, value))
  {
    get_next_char(code, cur_char);
  }
  else
  {
    std::string errmsg = std::string("Expecting .:| ") + char(value) + " |:. , but got .:| " + char(cur_char) + " |:.";
    throw StyioSyntaxError(errmsg);
  }
}

bool peak_next_char (
  struct StyioCodeContext* code,
  char value,
  int mode = 0)
{
  int start_with = code -> cursor;
  int move_forward = 0;

  switch (mode)
  {
  case 1:
    while ((code -> text.at(start_with + move_forward)) == ' ')
    {
      move_forward += 1;
    }
    
    break;

  case 2:
    while (isspace((code -> text.at(start_with + move_forward))))
    {
      move_forward += 1;
    }

    break;
  
  default:
    break;
  }

  return (code -> text.at(start_with + move_forward)) == value;
}

/*
  =================
*/


IdAST* parse_id (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  std::string idStr = "";

  // [a-zA-Z][a-zA-Z0-9_]*
  do {
    idStr += cur_char;
    get_next_char(code, cur_char);
  } while (
    isalnum((cur_char)) 
    || check_this_char(cur_char, '_')
  );

  return new IdAST(idStr);
}

IntAST* parse_int (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  std::string intStr = "";

  // push the current character into string
  intStr += cur_char;

  // progress to the next
  get_next_char(code, cur_char);

  // [0-9]*
  while (isdigit(cur_char))
  {
    intStr += cur_char;
    get_next_char(code, cur_char);
  };

  return new IntAST(std::stoi(intStr));
}

StyioAST* parse_int_or_float (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  std::string numStr = "";
  numStr += cur_char;
  get_next_char(code, cur_char);

  // [0-9]*
  while (isdigit(cur_char))
  {
    numStr += cur_char;
    get_next_char(code, cur_char);
  };

  if (check_this_char(cur_char, '.')) 
  {
    get_next_char(code, cur_char);

    if (isdigit(cur_char))
    {
      numStr += '.';

      numStr += cur_char;

      while (isdigit(cur_char))
      {
        numStr += cur_char;
        get_next_char(code, cur_char);
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
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  // eliminate the first(start) double quote
  get_next_char(code, cur_char);

  std::string textStr = "";
  
  while (cur_char != '\"')
  {
    textStr += cur_char;
    get_next_char(code, cur_char);
  };

  // eliminate the second(end) double quote
  get_next_char(code, cur_char);

  return new StringAST(textStr);
}

StyioAST* parse_char_or_string (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  // eliminate the first(start) single quote
  get_next_char(code, cur_char);

  std::string textStr = "";
  
  while (cur_char != '\'')
  {
    textStr += cur_char;
    get_next_char(code, cur_char);
  };

  // eliminate the second(end) single quote
  get_next_char(code, cur_char);

  if (textStr.length() == 1)
  {
    return new CharAST(textStr);
  }
  else
  {
    return new StringAST(textStr);
  }
}

SizeOfAST* parse_size_of (
  struct StyioCodeContext* code, 
  int& cur_char
) 
{
  // eliminate | at the start
  get_next_char(code, cur_char);
       
  if (isalpha(cur_char) || check_this_char(cur_char, '_'))
  {
    IdAST* var = parse_id(code, cur_char);

    // eliminate | at the end
    if (check_this_char(cur_char, '|')) {
      get_next_char(code, cur_char);

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

StyioAST* parse_ext_res (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  // eliminate @
  get_next_char(code, cur_char);

  if (check_this_char(cur_char, '(')) {
    // eliminate (
    get_next_char(code, cur_char);

    if (check_this_char(cur_char, '\"')) {
      // eliminate the left double quote "
      get_next_char(code, cur_char);

      std::string textStr = "";
  
      while (cur_char != '\"')
      {
        textStr += cur_char;
        get_next_char(code, cur_char);
      };

      if (check_this_char(cur_char, '\"')) {
        // eliminate the right double quote "
        get_next_char(code, cur_char);
      }
      else
      {
        std::string errmsg = std::string("Expecting \" at the end, but got `") + char(cur_char) + "`.";
        throw StyioSyntaxError(errmsg);
      };

      if (check_this_char(cur_char, ')')) {
        // eliminate )
        get_next_char(code, cur_char);
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

StyioAST* parse_list_elem (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  if (isdigit(cur_char)) 
  {
    return parse_int_or_float(code, cur_char);
  }
  else if (isalpha(cur_char) || check_this_char(cur_char, '_')) 
  {
    return parse_id(code, cur_char);
  }
  else if (check_this_char(cur_char, '\"')) 
  {
    return parse_string(code, cur_char);
  }
  else if (check_this_char(cur_char, '\'')) 
  {
    return parse_char_or_string(code, cur_char);
  }
  
  std::string errmsg = std::string("Unexpected List / Range Element, starts with character `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

/*
  List Operation

  | [*] get_index_by_item
    : [?= item]
  
  | [*] insert_item_by_index
    : [+: index <- item]
  
  | [*] remove_item_by_index
    : [-: index]
  | [*] remove_many_items_by_indices
    : [-: (i0, i1, ...)]
  | [*] remove_item_by_value
    : [-: ?= item]
  | [ ] remove_many_items_by_values
    : [-: ?^ (v0, v1, ...)]

  | [*] get_reversed
    : [<]
  | [ ] get_index_by_item_from_right
    : [[<] ?= item]
  | [ ] remove_item_by_value_from_right
    : [[<] -: ?= value]
  | [ ] remove_many_items_by_values_from_right
    : [[<] -: ?^ (v0, v1, ...)]
*/

ListOpAST* parse_list_op (
  struct StyioCodeContext* code, 
  int& cur_char,
  StyioAST* theList
) 
{
  // eliminate [ at the start
  get_next_char(code, cur_char);

  ListOpAST* listop;

  if (isdigit(cur_char))
  {
    IntAST* indexExpr = parse_int(code, cur_char);
    listop = new ListOpAST(
      theList, 
      ListOpType::Access_Via_Index,
      indexExpr);
  }
  else
  {
    switch (cur_char)
    {
    case '"':
      {
        /*
          list["any"]
        */

        StyioAST* strExpr = parse_string(code, cur_char);
        
        listop = new ListOpAST(
          theList, 
          ListOpType::Access_Via_Name,
          strExpr);
      }
      
      // You should NOT reach this line!
      break;

    case '<':
      {
        /*
          list[<]
        */

        get_next_char(code, cur_char);

        listop = new ListOpAST(
          theList, 
          ListOpType::Get_Reversed);
      }

      // You should NOT reach this line!
      break;

    // list[?= item]
    case '?':
      {
        get_next_char(code, cur_char);

        if (check_this_char(cur_char, '='))
        {
          get_next_char(code, cur_char);

          StyioAST* theItem = parse_list_elem(code, cur_char);

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
    
    case '+':
      {
        get_next_char(code, cur_char);

        if (check_this_char(cur_char, ':'))
        {
          get_next_char(code, cur_char);

          // eliminate white spaces after +:
          drop_white_spaces(code, cur_char);

          if (isdigit(cur_char))
          {
            /*
              list[+: index <- value]
            */

            IntAST* theIndex = parse_int(code, cur_char);

            // eliminate white spaces between index and <-
            drop_white_spaces(code, cur_char);

            if (check_this_char(cur_char, '<'))
            {
              get_next_char(code, cur_char);

              if (check_this_char(cur_char, '-'))
              {
                get_next_char(code, cur_char);

                // eliminate white spaces between <- and the value to be inserted
                drop_white_spaces(code, cur_char);

                // the item to be inserted into the list
                StyioAST* theItemIns = parse_list_elem(code, cur_char);

                listop = new ListOpAST(
                  theList, 
                  ListOpType::Insert_Item_By_Index,
                  theIndex,
                  theItemIns);
              }
              else
              {
                std::string errmsg = std::string("Missing `-` for `<-` after `+: index`, got `") + char(cur_char) + "`";
                throw StyioSyntaxError(errmsg);
              };
            }
            else
            {
              std::string errmsg = std::string("Expecting `<-` after `+: index`, but got `") + char(cur_char) + "`";
              throw StyioSyntaxError(errmsg);
            }
          }
        }
        else
        {
          std::string errmsg = std::string("Expecting integer index after `+:`, but got `") + char(cur_char) + "`";
          throw StyioSyntaxError(errmsg);
        }
      }

      // You should NOT reach this line!
      break;
    
    case '-':
      {
        get_next_char(code, cur_char);

        if (check_this_char(cur_char, ':'))
        {
          get_next_char(code, cur_char);

          // eliminate white spaces after -:
          drop_white_spaces(code, cur_char);

          if (isdigit(cur_char))
          {
            /*
              list[-: index]
            */

            IntAST* theIndex = parse_int(code, cur_char);

            // eliminate white spaces between index
            drop_white_spaces(code, cur_char);

            listop = new ListOpAST(
              theList, 
              ListOpType::Remove_Item_By_Index,
              theIndex);
          }
          else
          {
            switch (cur_char)
            {
            case '(':
            {
              /*
                list[-: (i0, i1, ...)]
              */

              // eliminate ( at the start
              get_next_char(code, cur_char);
              
              // drop white spaces between '(' and the first index
              drop_white_spaces(code, cur_char);

              std::vector<IntAST*> indices;

              IntAST* firstIndex = parse_int(code, cur_char);
              indices.push_back(firstIndex);

              // drop white spaces between first index and ,
              drop_white_spaces(code, cur_char);

              while (check_this_char(cur_char, ','))
              {
                // remove ,
                get_next_char(code, cur_char);

                // drop white spaces between , and next index
                drop_white_spaces(code, cur_char);

                if (check_this_char(cur_char, ')'))
                {
                  break;
                }

                IntAST* nextIndex = parse_int(code, cur_char);
                indices.push_back(nextIndex);
              }

              // drop white spaces between , and )
              drop_white_spaces(code, cur_char);
              
              if (check_this_char(cur_char, ')'))
              {
                get_next_char(code, cur_char);

                listop = new ListOpAST(
                  theList, 
                  ListOpType::Remove_Many_Items_By_Indices,
                  indices);
              }
              else
              {
                std::string errmsg = std::string("Expecting `)` after `-: (i0, i1, ...`, but got `") + char(cur_char) + "`";
                throw StyioSyntaxError(errmsg);
              }
            }

              // You should NOT reach this line!
              break;

            case '?':
            {
              get_next_char(code, cur_char);

              switch (cur_char)
              {
              case '=':
              {
                /*
                  list[-: ?= value]
                */

                get_next_char(code, cur_char);

                // drop white spaces after ?=
                drop_white_spaces(code, cur_char);
                
                StyioAST* valExpr = parse_list_elem(code, cur_char);

                listop = new ListOpAST(
                  theList, 
                  ListOpType::Remove_Item_By_Value,
                  valExpr);
              }
              
                // You should NOT reach this line!
                break;
              
              case '^':
              {
                /*
                  list[-: ?^ (v0, v1, ...)]
                */

                get_next_char(code, cur_char);

                // drop white spaces after ?^
                drop_white_spaces(code, cur_char);

                if (check_this_char(cur_char, '(') 
                  || check_this_char(cur_char, '[')
                  || check_this_char(cur_char, '{'))
                {
                  get_next_char(code, cur_char);
                }
              }
              
              // You should NOT reach this line!
              break;
              
              default:
                break;
              }
            }
            
            default:
              break;
            }
          }
        }
        else
        {
          std::string errmsg = std::string("Missing `:` for `-:`, got `") + char(cur_char) + "`";
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
  }

  // check ] at the end
  if (check_this_char(cur_char, ']'))
  {
    // eliminate ] at the end
    get_next_char(code, cur_char);
    
    return listop;
  }
  else
  {
    std::string errmsg = std::string("Missing `]` after List[Operation], got `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

std::vector<IdAST*> parse_multi_vars (
  struct StyioCodeContext* code, 
  int& cur_char
) 
{
  std::vector<IdAST*> vars;

  IdAST* firstVar = parse_id(code, cur_char);

  vars.push_back(firstVar);

  drop_white_spaces(code, cur_char);

  while (check_this_char(cur_char, ','))
  {
    get_next_char(code, cur_char);

    drop_white_spaces(code, cur_char);

    /*
      the last character will be eliminated outside parse_multi_vars()
      therefore, this function only eliminate variable declaration
    */
    if (check_this_char(cur_char, ')')  
      || check_this_char(cur_char, ']')
      || check_this_char(cur_char, '|'))
    {
      break;
    };

    vars.push_back(parse_id(code, cur_char));
  }

  return vars;
}

StyioAST* parse_iter (
  struct StyioCodeContext* code, 
  int& cur_char,
  StyioAST* iterOverIt
) 
{
  std::vector<IdAST*> iterTmpVars;
  StyioAST* iterMatch;
  StyioAST* iterFilter;
  StyioAST* iterBlock;

  bool hasMatch = false;
  bool hasFilter = false;

  // eliminate the start
  switch (cur_char)
  {
  case '(':
    get_next_char(code, cur_char);
    break;

  case '[':
    get_next_char(code, cur_char);
    break;

  case '|':
    get_next_char(code, cur_char);
    break;
  
  default:
    break;
  }

  drop_white_spaces(code, cur_char);

  iterTmpVars = parse_multi_vars(code, cur_char);

  drop_white_spaces(code, cur_char);

  // eliminate the end
  switch (cur_char)
  {
  case ')':
    get_next_char(code, cur_char);
    break;

  case ']':
    get_next_char(code, cur_char);
    break;

  case '|':
    get_next_char(code, cur_char);
    break;
  
  default:
    break;
  }

  /*
    support:
    
    (x, y) \n
    ?=

    (x, y) \n
    =>

  */

  drop_all_spaces(code, cur_char);

  if (check_this_char(cur_char, '?'))
  {
    get_next_char(code, cur_char);

    switch (cur_char)
    {
    /*
      ?= Value
    */
    case '=':
      {
        get_next_char(code, cur_char);

        // drop white spaces after ?=
        drop_white_spaces(code, cur_char);

        StyioAST* matchValue = parse_simple_value(code, cur_char);
        
        iterMatch = new CheckEqAST(matchValue);
        hasMatch = true;
      }

      break;
    /*
      ?^ [Iterable]
    */
    case '^':
      {

      }

      break;

    /*
      ?(Condition) 
      :) {

      }
      
      ?(Condition) 
      :( {

      }
    */
    case '(':
      {

      }

      break;
    
    default:
      std::string errmsg = std::string("Unexpected character after ?: `") + char(cur_char) + "`";
      throw StyioSyntaxError(errmsg);

      break;
    }
  };

  if (check_this_char(cur_char, '='))
  {
    get_next_char(code, cur_char);

    if (check_this_char(cur_char, '>'))
    {
      get_next_char(code, cur_char);

      /*
        support:

        => \n
        { }
      */
      drop_all_spaces(code, cur_char);

      if (check_this_char(cur_char, '{'))
      {
        iterBlock = parse_exec_block(code, cur_char);
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
    return new IterInfiniteAST(iterTmpVars, iterBlock);

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

StyioAST* parse_list_expr (
  struct StyioCodeContext* code, 
  int& cur_char
) 
{
  std::vector<StyioAST*> elements;

  StyioAST* startEl = parse_list_elem(code, cur_char);
  elements.push_back(startEl);

  drop_white_spaces(code, cur_char);

  switch (cur_char)
  {
  case '.':
    {
      get_next_char(code, cur_char);

      while (check_this_char(cur_char, '.'))
      {
        get_next_char(code, cur_char);
      }
      
      StyioAST* endEl = parse_list_elem(code, cur_char);

      StyioAST* list_loop;

      if (startEl -> hint() == StyioType::Int 
        && endEl -> hint() == StyioType::Id)
      {
        list_loop = new InfiniteAST(startEl, endEl);
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
        get_next_char(code, cur_char);
      }
      else
      {
        std::string errmsg = std::string("Missing `]` after List / Range / Loop: `") + char(cur_char) + "`";
        throw StyioSyntaxError(errmsg);
      };

      drop_white_spaces(code, cur_char);

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
          get_next_char(code, cur_char);

          if (check_this_char(cur_char, '>'))
          {
            // If: >>, Then: Iteration
            get_next_char(code, cur_char);
            
            return parse_iter(code, cur_char, list_loop);
          }
        }
        
        // You should NOT reach this line!
        break;

      case '[':
        {
          return parse_list_op(code, cur_char, list_loop);
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
        get_next_char(code, cur_char);

        drop_white_spaces(code, cur_char);

        if (check_this_char(cur_char, ']')) 
        {
          get_next_char(code, cur_char);

          theList = new ListAST(elements);
        };

        StyioAST* el = parse_list_elem(code, cur_char);

        elements.push_back(el);
      };

      drop_white_spaces(code, cur_char);

      if (check_this_char(cur_char, ']')) 
      {
        get_next_char(code, cur_char);

        theList = new ListAST(elements);
      }
      else
      {
        std::string errmsg = std::string("Missing `]` after List / Range / Loop: `") + char(cur_char) + "`";
        throw StyioSyntaxError(errmsg);
      };

      drop_white_spaces(code, cur_char);

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
          get_next_char(code, cur_char);

          if (check_this_char(cur_char, '>'))
          {
            // If: >>, Then: Iteration
            get_next_char(code, cur_char);

            return parse_iter(code, cur_char, theList);
          }

          // TODO: Iteration Over List / Range / Loop

          return theList;
        }
        
        // You should NOT reach this line!
        break;

      case '[':
        {
          return parse_list_op(code, cur_char, theList);
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

StyioAST* parse_loop (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  while (check_this_char(cur_char, '.')) 
  { 
    // eliminate all .
    get_next_char(code, cur_char);

    if (check_this_char(cur_char, ']')) 
    {
      get_next_char(code, cur_char);

      // return new InfiniteAST();
      break;
    };
  };

  // eliminate white spaces after [...]
  drop_white_spaces(code, cur_char);

  if (isdigit(cur_char))
  {
    StyioAST* errnum = parse_int_or_float(code, cur_char);
    
    std::string errmsg = std::string("A finite list must have both start and end values. However, only the end value is detected: `") + errnum -> toStringInline() + "`. Try `[0.." + errnum -> toStringInline() + "]` rather than `[.." + errnum -> toStringInline() + "]`.";
    throw StyioSyntaxError(errmsg);
  }

  switch (cur_char)
  {
  case '>':
    {
      get_next_char(code, cur_char);

      if (check_this_char(cur_char, '>'))
      {
        get_next_char(code, cur_char);

        // eliminate white spaces after >>
        drop_white_spaces(code, cur_char);

        if (isalpha(cur_char) 
            || check_this_char(cur_char, '_')
            || check_this_char(cur_char, '(')) 
        {
          return parse_iter(code, cur_char, new InfiniteAST());
        }
        else
        if (check_this_char(cur_char, '{'))
        {
          /*
            the { at the start will be eliminated inside parse_exec_block() function
          */
          StyioAST* block = parse_exec_block(code, cur_char);

          return new IterInfiniteAST(block);
        }
      }
    }

    // You should not reach this line!
    break;

  case '(':
    {
      return parse_iter(code, cur_char, new InfiniteAST());
    }

    // You should not reach this line!
    break;
  
  case '\n':
    {
      return new InfiniteAST();
    }

    // You should not reach this line!
    break;

  default:
    break;
  }

  std::string errmsg = std::string("Unexpected character after infinite loop: `") + char(cur_char) + "` in infinite expression.";
  throw StyioSyntaxError(errmsg);
}

StyioAST* parse_val_for_binop (
  struct StyioCodeContext* code, 
  int& cur_char
) 
{
  drop_white_spaces(code, cur_char);

  // ID
  if (isalpha(cur_char) || check_this_char(cur_char, '_')) {
    IdAST* result = parse_id(code, cur_char);
    return result;
  }
  else
  // Int / Float
  if (isdigit(cur_char)) {
    StyioAST* result = parse_int_or_float(code, cur_char);
    return result;
  }
  else
  {
    switch (cur_char)
    {
    // List
    case '[':
      {
        get_next_char(code, cur_char);

        if (check_this_char(cur_char, ']')) {
          get_next_char(code, cur_char);

          return new EmptyListAST();
        }
        else
        {
          return parse_list_expr(code, cur_char);
        }
      }

      // You should NOT reach this line!
      break;

    // SizeOf()
    case '|':
      {
        return parse_size_of(code, cur_char);
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

BinOpAST* parse_binop_rhs (
  struct StyioCodeContext* code, 
  int& cur_char, 
  StyioAST* lhs_ast
) 
{
  BinOpAST* binOp;

  drop_all_spaces(code, cur_char);

  switch (cur_char)
  {
    // BIN_ADD := <ID> "+" <EXPR>
    case '+':
      {
        get_next_char(code, cur_char);

        // <ID> "+" |-- 
        binOp = new BinOpAST(
          BinOpType::BIN_ADD, 
          lhs_ast, 
          parse_val_for_binop(code, cur_char));
      };

      // You should NOT reach this line!
      break;

    // BIN_SUB := <ID> "-" <EXPR>
    case '-':
      {
        get_next_char(code, cur_char);

        // <ID> "-" |--
        binOp = new BinOpAST(
          BinOpType::BIN_SUB, 
          lhs_ast, 
          parse_val_for_binop(code, cur_char));
      };

      // You should NOT reach this line!
      break;

    // BIN_MUL | BIN_POW
    case '*':
      {
        get_next_char(code, cur_char);
        // BIN_POW := <ID> "**" <EXPR>
        if (check_this_char(cur_char, '*'))
        {
          get_next_char(code, cur_char);

          // <ID> "**" |--
          binOp = new BinOpAST(
            BinOpType::BIN_POW, 
            lhs_ast, 
            parse_val_for_binop(code, cur_char));
        } 
        // BIN_MUL := <ID> "*" <EXPR>
        else 
        {
          // <ID> "*" |--
          binOp = new BinOpAST(
            BinOpType::BIN_MUL, 
            lhs_ast, 
            parse_val_for_binop(code, cur_char));
        }
      };
      // You should NOT reach this line!
      break;
      
    // BIN_DIV := <ID> "/" <EXPR>
    case '/':
      {
        get_next_char(code, cur_char);

        // <ID> "/" |-- 
        binOp = new BinOpAST(
          BinOpType::BIN_DIV, 
          lhs_ast, 
          parse_val_for_binop(code, cur_char));
      };

      // You should NOT reach this line!
      break;

    // BIN_MOD := <ID> "%" <EXPR> 
    case '%':
      {
        get_next_char(code, cur_char);

        // <ID> "%" |-- 
        binOp = new BinOpAST(
          BinOpType::BIN_MOD, 
          lhs_ast, 
          parse_val_for_binop(code, cur_char));
      };

      // You should NOT reach this line!
      break;
    
    default:
      std::string errmsg = std::string("Unexpected BinOp.Operator: `") + char(cur_char) + "`.";
      throw StyioSyntaxError(errmsg);
  }

  while (cur_char != '\n') 
  {
    binOp = parse_binop_rhs(code, cur_char, binOp);
  }

  return binOp;
}

/*
  parse_val_for_cond

  either:
    simple_value
  or:
    simple_value == simple_value
                 != 
                 >= 
                 >  
                 <= 
                 <  
*/

StyioAST* parse_val_for_cond (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  StyioAST* valExpr;

  // drop all spaces first value
  drop_all_spaces(code, cur_char);

  valExpr = parse_simple_value(code, cur_char);
  
  // drop all spaces after first value
  drop_all_spaces(code, cur_char);

  switch (cur_char)
  {
  case '=':
    {
      get_next_char(code, cur_char);

      if (check_this_char(cur_char, '='))
      {
        get_next_char(code, cur_char);

        /*
          Equal
            expr == expr
        */

        // drop all spaces after ==
        drop_all_spaces(code, cur_char);
        
        valExpr = new BinCompAST(
          CompType::EQ,
          valExpr,
          parse_simple_value(code, cur_char));
      };
    }

    break;

  case '!':
    {
      get_next_char(code, cur_char);

      if (check_this_char(cur_char, '='))
      {
        get_next_char(code, cur_char);

        /*
          Not Equal
            expr != expr
        */

        // drop all spaces after !=
        drop_all_spaces(code, cur_char);

        valExpr = new BinCompAST(
          CompType::NE,
          valExpr,
          parse_simple_value(code, cur_char));
      };
    }

    break;

  case '>':
    {
      get_next_char(code, cur_char);

      if (check_this_char(cur_char, '='))
      {
        get_next_char(code, cur_char);

        /*
          Greater Than and Equal
            expr >= expr
        */

        // drop all spaces after >=
        drop_all_spaces(code, cur_char);

        valExpr = new BinCompAST(
          CompType::GE,
          valExpr,
          parse_simple_value(code, cur_char));
      }
      else
      {
        /*
          Greater Than
            expr > expr
        */

        // drop all spaces after >
        drop_all_spaces(code, cur_char);

        valExpr = new BinCompAST(
          CompType::GT,
          valExpr,
          parse_simple_value(code, cur_char));
      };
    }

    break;

  case '<':
    {
      get_next_char(code, cur_char);

      if (check_this_char(cur_char, '='))
      {
        get_next_char(code, cur_char);

        /*
          Less Than and Equal
            expr <= expr
        */

        // drop all spaces after <=
        drop_all_spaces(code, cur_char);

        valExpr = new BinCompAST(
          CompType::LE,
          valExpr,
          parse_simple_value(code, cur_char));
      }
      else
      {
        /*
          Less Than
            expr < expr
        */

        // drop all spaces after <
        drop_all_spaces(code, cur_char);

        valExpr = new BinCompAST(
          CompType::LT,
          valExpr,
          parse_simple_value(code, cur_char));
      };
    }

    break;

  default:
    break;
  }

  return valExpr;
}

CondAST* parse_cond_rhs (
  struct StyioCodeContext* code, 
  int& cur_char,
  StyioAST* lhsExpr
)
{
  CondAST* condExpr;

  drop_all_spaces(code, cur_char);

  switch (cur_char)
  {
  case '&':
    {
      get_next_char(code, cur_char);

      if (check_this_char(cur_char, '&'))
      {
        get_next_char(code, cur_char);
      };

      /*
        support:
          expr && \n
          expression
      */

      drop_all_spaces(code, cur_char);

      condExpr = new CondAST(
        LogicType::AND,
        lhsExpr,
        parse_cond(code, cur_char)
      );
    }

    break;

  case '|':
    {
      get_next_char(code, cur_char);

      if (check_this_char(cur_char, '|'))
      {
        get_next_char(code, cur_char);
      };

      /*
        support:
          expr || \n
          expression
      */

      drop_all_spaces(code, cur_char);

      condExpr = new CondAST(
        LogicType::OR,
        lhsExpr,
        parse_cond(code, cur_char)
      );
    }

    break;

  case '^':
    {
      get_next_char(code, cur_char);

      /*
        support:
          expr ^ \n
          expression
      */

      drop_all_spaces(code, cur_char);

      condExpr = new CondAST(
        LogicType::OR,
        lhsExpr,
        parse_cond(code, cur_char)
      );
    }

    break;

  case '!':
    {
      get_next_char(code, cur_char);

      if (check_this_char(cur_char, '('))
      {
        get_next_char(code, cur_char);

        /*
          support:
            !( \n
              expr
            )
        */
        drop_all_spaces(code, cur_char);

        condExpr = new CondAST(
          LogicType::NOT,
          parse_cond(code, cur_char)
        );

        check_and_drop(code, cur_char, ')', 2);
      }
    }

    break;

  default:
    break;
  }

  drop_all_spaces(code, cur_char);

  while (!(check_this_char(cur_char, ')')))
  {
    condExpr = parse_cond_rhs(code, cur_char, condExpr);
  }
  
  return condExpr;
}

CondAST* parse_cond (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  StyioAST* lhsExpr;

  if (check_this_char(cur_char, '('))
  {
    get_next_char(code, cur_char);

    lhsExpr = parse_cond(code, cur_char);

    check_and_drop(code, cur_char, ')', 2);
  }
  else
  if (check_this_char(cur_char, '!'))
  {
    get_next_char(code, cur_char);

    if (check_this_char(cur_char, '('))
    {
      get_next_char(code, cur_char);

      /*
        support:
          !( \n
            expr
          )
      */
      drop_all_spaces(code, cur_char);

      lhsExpr = parse_cond(code, cur_char);

      drop_all_spaces(code, cur_char);

      return new CondAST(
        LogicType::NOT,
        lhsExpr
      );
    }
    else
    {
      std::string errmsg = std::string("!(expr) // Expecting ( after !, but got ") + char(cur_char);
      throw StyioSyntaxError(errmsg);
    };
  }
  else
  {
    lhsExpr = parse_val_for_cond(code, cur_char);
  };

  // drop all spaces after first value
  drop_all_spaces(code, cur_char);

  if (check_this_char(cur_char, '&')
    || check_this_char(cur_char, '|'))
  {
    return parse_cond_rhs(code, cur_char, lhsExpr);
  }
  else
  {
    return new CondAST(
      LogicType::RAW,
      lhsExpr
    );
  }

  std::string errmsg = std::string("parse_cond() : You should not reach this line!") + char(cur_char);
  throw StyioParseError(errmsg);
}

StyioAST* parse_cond_flow (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  // eliminate ?
  get_next_char(code, cur_char);

  CondAST* condition;
  
  if (check_this_char(cur_char, '(')) {
    get_next_char(code, cur_char);

    condition = parse_cond(code, cur_char);

    check_and_drop(code, cur_char, ')', 2);

    /*
      support:
        ?() \n
        \t\

        ?() \n
        \f\
    */
    drop_all_spaces(code, cur_char);

    if (check_this_char(cur_char, '\\'))
    {
      get_next_char(code, cur_char);

      StyioAST* block;

      if (check_this_char(cur_char, 't'))
      {
        get_next_char(code, cur_char);

        check_and_drop(code, cur_char, '\\', 0);

        /*
          support:
            \t\ \n
            {}
        */

        drop_all_spaces(code, cur_char);

        block = parse_exec_block(code, cur_char);

        /*
          support:
            \t\ {} \n
            \f\
        */
        drop_all_spaces(code, cur_char);

        if (check_this_char(cur_char, '\\'))
        {
          get_next_char(code, cur_char);

          check_and_drop(code, cur_char, 'f', 0);

          if (check_this_char(cur_char, '\\'))
          {
            get_next_char(code, cur_char);

            /*
              support:
                \f\ \n
                {}
            */
            drop_all_spaces(code, cur_char);

            StyioAST* blockElse = parse_exec_block(code, cur_char);

            return new CondFlowAST(
              FlowType::TrueAndFalse,
              condition,
              block,
              blockElse
            );
          };
        }
        else
        {
          return new CondFlowAST(
            FlowType::OnlyTrue,
            condition,
            block
          );
        };
      }
      else if (check_this_char(cur_char, 'f'))
      {
        get_next_char(code, cur_char);

        check_and_drop(code, cur_char, '\\', 0);

        /*
          support:
            \f\ \n
            {}
        */
        drop_all_spaces(code, cur_char);

        block = parse_exec_block(code, cur_char);

        return new CondFlowAST(
          FlowType::OnlyFalse,
          condition,
          block
        );
      }
      else
      {

      }
    }
  }
  else 
  {
    std::string errmsg = std::string("Missing `(` for ?(`expr`).");
    throw StyioSyntaxError(errmsg);
  };

  return condition;
}

StyioAST* parse_simple_value (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  if (isdigit(cur_char)) 
  {
    return parse_int_or_float(code, cur_char);
  }
  else 
  if (isalpha(cur_char) 
    || check_this_char(cur_char, '_')) 
  {
    return parse_id(code, cur_char);
  }

  switch (cur_char)
  {
  case '\"':
    return parse_string(code, cur_char);
    
    // You should NOT reach this line!
    break;
  
  case '\'':
    return parse_char_or_string(code, cur_char);

    // You should NOT reach this line!
    break;

  case '|':
    return parse_size_of(code, cur_char);

    // You should NOT reach this line!
    break;
  
  default:
    break;
  }

  std::string errmsg = std::string("parse_simple_value(), unexpected character `") + char(cur_char) + "`";
  throw StyioParseError(errmsg);
}

StyioAST* parse_expr (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  // <ID>
  if (isalpha(cur_char) || check_this_char(cur_char, '_')) 
  {
    // parse id
    IdAST* id_ast = parse_id(code, cur_char);
    
    // ignore white spaces after id
    drop_white_spaces(code, cur_char);

    if (is_bin_tok(cur_char))
    {
      return parse_binop_rhs(code, cur_char, id_ast);
    }
    else
    {
      return id_ast;
    }
  }
  else
  if (isdigit(cur_char)) {
    StyioAST* numAST = parse_int_or_float(code, cur_char);

    // ignore white spaces after number
    drop_white_spaces(code, cur_char);

    if (is_bin_tok(cur_char))
    {
      return parse_binop_rhs(code, cur_char, numAST);
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
        get_next_char(code, cur_char);

        drop_white_spaces(code, cur_char);

        if (check_this_char(cur_char, ']')) {
          get_next_char(code, cur_char);

          return new EmptyListAST();
        }
        else
        {
          return parse_list_expr(code, cur_char);
        }
      }

      // You should NOT reach this line!
      break;

    case '|':
      {
        SizeOfAST* valExpr = parse_size_of(code, cur_char);

        drop_white_spaces(code, cur_char);

        if (is_bin_tok(cur_char))
        {
          return parse_binop_rhs(code, cur_char, valExpr);
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

ResourceAST* parse_resources (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  std::vector<StyioAST*> resources;

  // eliminate @
  get_next_char(code, cur_char);
  
  drop_white_spaces(code, cur_char);

  if (check_this_char(cur_char, '(')) 
  {
    get_next_char(code, cur_char);

    drop_all_spaces(code, cur_char);

    if (isalpha(cur_char) || check_this_char(cur_char, '_')) {
      // "@" "(" |--
      IdAST* tmp_var = parse_id(code, cur_char);

      drop_white_spaces(code, cur_char);

      if (check_this_char(cur_char, '<'))
      {
        get_next_char(code, cur_char);

        check_and_drop(code, cur_char, '-');

        drop_white_spaces(code, cur_char);

        StyioAST* tmp_value = parse_simple_value(code, cur_char);

        StyioAST* binding = new FinalBindAST(tmp_var, tmp_value);

        resources.push_back(binding);
      }
      else
      {
        resources.push_back(tmp_var);
      };
    };

    drop_white_spaces(code, cur_char);

    // "@" "(" [<ID> |--
    while (check_this_char(cur_char, ','))
    {
      get_next_char(code, cur_char);

      drop_all_spaces(code, cur_char);

      if (isalpha(cur_char) || check_this_char(cur_char, '_')) {
        
        IdAST* tmp_var = parse_id(code, cur_char);

        drop_white_spaces(code, cur_char);

        if (check_this_char(cur_char, '<'))
        {
          get_next_char(code, cur_char);

          check_and_drop(code, cur_char, '-');

          drop_white_spaces(code, cur_char);
          
          StyioAST* tmp_value = parse_simple_value(code, cur_char);

          StyioAST* binding = new FinalBindAST(tmp_var, tmp_value);

          resources.push_back(binding);
        }
        else
        {
          resources.push_back(tmp_var);
        };
      };
    };
    
    if (check_this_char(cur_char, ')')) 
    {
      get_next_char(code, cur_char);

      return new ResourceAST(resources);
    }
    else
    {
      std::string errmsg = std::string("@(expr) // Expecting ) at the end, but got ") + char(cur_char) + "";
      throw StyioSyntaxError(errmsg);
    };
  }
  else
  {
    std::string errmsg = std::string("@(expr) // Expecting ( after @, but got ") + char(cur_char) + "";
    throw StyioSyntaxError(errmsg);
  };

  std::string errmsg = std::string("@(expr) // Something wrong, got ") + char(cur_char) + "";
  throw StyioSyntaxError(errmsg);
}

FlexBindAST* parse_mut_assign (
  struct StyioCodeContext* code, 
  int& cur_char, 
  IdAST* id_ast
)
{
  FlexBindAST* output = new FlexBindAST(id_ast, parse_expr(code, cur_char));
  
  if (check_this_char(cur_char, '\n')) 
  {
    return output;
  }
  else
  {
    std::string errmsg = std::string("Unexpected character after binding (flexible): ") + char(cur_char);
    throw StyioSyntaxError(errmsg);
  }
}

FinalBindAST* parse_fix_assign (
  struct StyioCodeContext* code, 
  int& cur_char, 
  IdAST* id_ast
) 
{
  FinalBindAST* output = new FinalBindAST(id_ast, parse_expr(code, cur_char));
  
  if (check_this_char(cur_char, '\n')) 
  {
    return output;
  }
  else
  {
    std::string errmsg = std::string("Unexpected character after binding (fixed): `") + char(cur_char) + "` after Assign(Mutable)";
    throw StyioSyntaxError(errmsg);
  }
}

StyioAST* parse_pipeline (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  IdAST* pipeName;
  std::vector<IdAST*> pipeVars;
  StyioAST* pipeBlock;
  bool pwithName = false;
  bool pisFinal = false;

  // eliminate # at the start
  get_next_char(code, cur_char);

  // after #
  drop_white_spaces(code, cur_char);

  if (isalpha(cur_char) 
    || check_this_char(cur_char, '_'))
  {
    pipeName = parse_id(code, cur_char);

    pwithName = true;
  };
  
  // after function name
  drop_all_spaces(code, cur_char);

  if (check_this_char(cur_char, ':'))
  {
    get_next_char(code, cur_char);

    pisFinal = true;
  };

  if (check_this_char(cur_char, '='))
  {
    get_next_char(code, cur_char);
  };

  drop_all_spaces(code, cur_char);

  if (check_this_char(cur_char, '('))
  {
    get_next_char(code, cur_char);

    pipeVars = parse_multi_vars(code, cur_char);

    check_and_drop(code, cur_char, ')', 2);
  };

  drop_all_spaces(code, cur_char);
  
  if (check_this_char(cur_char, '='))
  {
    get_next_char(code, cur_char);

    if (check_this_char(cur_char, '>'))
    {
      get_next_char(code, cur_char);
    };
  };

  drop_all_spaces(code, cur_char);

  if (check_this_char(cur_char, '{'))
  {
    pipeBlock = parse_exec_block(code, cur_char);

    if (pwithName)
    {
      return new FuncAST(
        pipeName,
        pipeVars,
        pipeBlock,
        pisFinal
      );
    }
    else
    {
      return new FuncAST(
        pipeVars,
        pipeBlock,
        pisFinal
      );
    };
  }
}

StyioAST* parse_read_file (
  struct StyioCodeContext* code, 
  int& cur_char, 
  IdAST* id_ast
) 
{
  if (check_this_char(cur_char, '@'))
  {
    StyioAST* value = parse_ext_res(code, cur_char);

    return new ReadFileAST(id_ast, value);
  }
  else
  {
    std::string errmsg = std::string("Unexpected Read.Path, starts with character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

StyioAST* parse_write_stdout (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  StyioAST* result;

  // eliminate >
  get_next_char(code, cur_char);
  
  if (check_this_char(cur_char, '_')) {
    // eliminate _
    get_next_char(code, cur_char);

    drop_white_spaces(code, cur_char);

    if (check_this_char(cur_char, '('))
    {
      // eliminate (
      get_next_char(code, cur_char);

      drop_all_spaces(code, cur_char);

      result = new WriteStdOutAST(parse_expr(code, cur_char));

      drop_all_spaces(code, cur_char);

      if (check_this_char(cur_char, ')'))
      {
        get_next_char(code, cur_char);
      }
      else
      {
        std::string errmsg = std::string("Missing `)` for >_(), got `") + char(cur_char) + "`.";
        throw StyioSyntaxError(errmsg);
      };
    }
    else
    {
      std::string errmsg = std::string("Unrecognized `expr` for >_(`expr`), got `") + char(cur_char) + "`";
      throw StyioSyntaxError(errmsg);
    };
  };

  return result;
}

StyioAST* parse_stmt (
  struct StyioCodeContext* code, 
  int& cur_char
) 
{
  drop_all_spaces(code, cur_char);

  // <ID>
  if (isalpha(cur_char) 
    || check_this_char(cur_char, '_')) 
  {
    // parse id
    IdAST* id_ast = parse_id(code, cur_char);

    if (check_this_char(cur_char, '['))
    {
      return parse_list_op(code, cur_char, id_ast);
    }
    
    // ignore white spaces after id
    drop_white_spaces(code, cur_char);

    // check next character
    switch (cur_char)
    {
      // <LF>
      case '\n':
        {
          get_next_char(code, cur_char);

          return id_ast;
        };

        // You should NOT reach this line!
        break;

      // <ID> = <EXPR>
      case '=':
        {
          get_next_char(code, cur_char);

          drop_white_spaces(code, cur_char);

          // <ID> = |--
          return parse_mut_assign(code, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;
      
      // <ID> := <EXPR>
      case ':':
        {
          get_next_char(code, cur_char);
          if (check_this_char(cur_char, '='))
          {
            get_next_char(code, cur_char);

            drop_white_spaces(code, cur_char);
            
            // <ID> := |--
            return parse_fix_assign(code, cur_char, id_ast);
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
          get_next_char(code, cur_char);

          if (check_this_char(cur_char, '-'))
          {
            // eliminate -
            get_next_char(code, cur_char);

            drop_white_spaces(code, cur_char);
            
            // <ID> <- |--
            return parse_read_file(code, cur_char, id_ast);
          }
          else
          {
            std::string errmsg = std::string("Expecting `-` after `<`, but found `") + char(cur_char) + "`.";
            throw StyioSyntaxError(errmsg);
          }
        };

        // You should NOT reach this line!
        break;

      // ID >> Layer
      case '>':
        {
          get_next_char(code, cur_char);

          if (check_this_char(cur_char, '>'))
          {
            // If: >>, Then: Iteration
            get_next_char(code, cur_char);
            
            return parse_iter(code, cur_char, id_ast);
          }
        }
        
        // You should NOT reach this line!
        break;

      // BIN_ADD := <ID> "+" <EXPR>
      case '+':
        {
          // <ID> |-- 
          return parse_binop_rhs(code, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;

      // BIN_SUB := <ID> "-" <EXPR>
      case '-':
        {
          // <ID> |--
          return parse_binop_rhs(code, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;

      // BIN_MUL | BIN_POW
      case '*':
        {
          // <ID> |--
          return parse_binop_rhs(code, cur_char, id_ast);
        };
        // You should NOT reach this line!
        break;
        
      // BIN_DIV := <ID> "/" <EXPR>
      case '/':
        {
          // <ID> |-- 
          return parse_binop_rhs(code, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;

      // BIN_MOD := <ID> "%" <EXPR> 
      case '%':
        {
          // <ID> |-- 
          return parse_binop_rhs(code, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;

      // LIST_OP |--
      case '[':
        {
          // <ID> |-- 

          return parse_list_op(code, cur_char, id_ast);
        };

        // You should NOT reach this line!
        break;
      
      default:
        break;
    }
  }

  if (isdigit(cur_char)) {
    StyioAST* numAST = parse_int_or_float(code, cur_char);

    drop_all_spaces(code, cur_char);

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
          BinOpAST* bin_ast = parse_binop_rhs(code, cur_char, numAST);
          return bin_ast;
        };

        // You should NOT reach this line!
        break;

      // BIN_SUB := [<Int>|<Float>] "-" <EXPR>
      case '-':
        {
          // [<Int>|<Float>] |--
          BinOpAST* bin_ast = parse_binop_rhs(code, cur_char, numAST);
          return bin_ast;
        };

        // You should NOT reach this line!
        break;

      // BIN_MUL | BIN_POW
      case '*':
        {
          // [<Int>|<Float>] |--
          BinOpAST* bin_ast = parse_binop_rhs(code, cur_char, numAST);
          return bin_ast;
        }

        // You should NOT reach this line!
        break;

      // BIN_DIV := [<Int>|<Float>] "/" <EXPR>
      case '/':
        {
          // [<Int>|<Float>] |--
          BinOpAST* bin_ast = parse_binop_rhs(code, cur_char, numAST);
          return bin_ast;
        };

        // You should NOT reach this line!
        break;

      // BIN_MOD := [<Int>|<Float>] "%" <EXPR>
      case '%':
        {
          // [<Int>|<Float>] |--
          BinOpAST* bin_ast = parse_binop_rhs(code, cur_char, numAST);
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
  case EOF:
    return new EndAST();

    // You should NOT reach this line!
    break;

  case '.':
    {
      while (check_this_char(cur_char, '.'))
      {
        get_next_char(code, cur_char);
      }
      
      return new PassAST();
    }

    // You should NOT reach this line!
    break;

  /*
    Resources

    @(expr <- expr)
  */
  case '@':
    {
      ResourceAST* resources = parse_resources(code, cur_char);

      if (peak_next_char(code, '-', 2))
      {
        check_and_drop(code, cur_char, '-', 2);

        check_and_drop(code, cur_char, '>', 0);

        drop_all_spaces(code, cur_char);

        StyioAST* block = parse_exec_block(code, cur_char);

        return new InjectAST(resources, block);
      }
      else
      {
        return resources;
      };
    };

    // You should NOT reach this line!
    break;
  
  /*
    "String"
  */
  case '\"':
    {
      return parse_string(code, cur_char);
    }

    break;

  /*
    Conditionals

    ?(expr) 
    :) {} 
    :( {}

    ?(expr) :( {}
  */
  case '?':
    {
      return parse_cond_flow(code, cur_char);
    }
    
    // You should NOT reach this line!
    break;

  /*
    List:
      [v0, v1, v2, ..., vn]
    
    Iterator:
      [...] >> {}

      List >> {}
  */
  case '[':
    {
      get_next_char(code, cur_char);

      if (check_this_char(cur_char, '.')) 
      {
        // eliminate the first dot .
        get_next_char(code, cur_char);

        if (check_this_char(cur_char, ']')) 
        {
          std::string errmsg = std::string("[.] is not infinite, please use [..] or [...] instead.");
          throw StyioSyntaxError(errmsg);
        };

        return parse_loop(code, cur_char);
      }
      else
      {
        return parse_list_expr(code, cur_char);
      }
    }
    
    // You should NOT reach this line!
    break;

  /*
    >_(expr)
  */
  case '>':
    {
      return parse_write_stdout(code, cur_char);
    }

    // You should NOT reach this line!
    break;

  /*
    # function 
      <: {
        interfaces
      } 
      := (items) => {
        statements
      }
  */
  case '#':
    {
      return parse_pipeline(code, cur_char);
    }

    // You should NOT reach this line!
    break;

  case '=':
    {
      get_next_char(code, cur_char);

      if (check_this_char(cur_char, '>'))
      {
        get_next_char(code, cur_char);
      };

      drop_white_spaces(code, cur_char);
      
      return new ReturnAST(parse_expr(code, cur_char));
    }

    // You should NOT reach this line!
    break;
    
  default:
    break;
  }

  std::string errmsg = std::string("Unrecognized statement, starting with `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

std::string parse_ext_elem(
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  std::string itemStr;

  if (check_this_char(cur_char, '\"'))
  {
    // eliminate double quote symbol " at the start of dependency item
    get_next_char(code, cur_char);

    while (cur_char != '\"') 
    {
      if (check_this_char(cur_char, ',')) 
      {
        std::string errmsg = std::string("An \" was expected after") + itemStr + "however, a delimeter `,` was detected. ";
        throw StyioSyntaxError(errmsg);
      }

      itemStr += cur_char;

      get_next_char(code, cur_char);
    };

    // eliminate double quote symbol " at the end of dependency item
    get_next_char(code, cur_char);

    return itemStr;
  }
  else
  {
    std::string errmsg = std::string("Dependencies should be wrapped with double quote like \"abc/xyz\", rather than starting with the character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

ExtPackAST* parse_ext_pack (
  struct StyioCodeContext* code, 
  int& cur_char
) 
{ 
  // eliminate left square (box) bracket [
  get_next_char(code, cur_char);

  std::vector<std::string> dependencies;

  drop_all_spaces(code, cur_char);

  // add the first dependency path to the list
  dependencies.push_back(parse_ext_elem(code, cur_char));

  std::string pathStr = "";
  
  while (check_this_char(cur_char, ',')) {
    // eliminate comma ","
    get_next_char(code, cur_char);

    // reset pathStr to empty ""
    pathStr = ""; 

    drop_all_spaces(code, cur_char);
    
    // add the next dependency path to the list
    dependencies.push_back(parse_ext_elem(code, cur_char));
  };

  if (check_this_char(cur_char, ']')) {
    // eliminate right square bracket `]` after dependency list
    get_next_char(code, cur_char);
  };

  ExtPackAST* result = new ExtPackAST(dependencies);

  return result;
}

StyioAST* parse_case_block (
  struct StyioCodeContext* code, 
  int& cur_char
)
{
  return new NoneAST();
}

StyioAST* parse_exec_block (
  struct StyioCodeContext* code, 
  int& cur_char
) 
{
  std::vector<StyioAST*> stmtBuffer;

  // eliminate { at the start
  get_next_char(code, cur_char);

  while (1)
  {
    drop_all_spaces(code, cur_char);
    
    if (check_this_char(cur_char, '}'))
    {
      // eliminate } at the end
      get_next_char(code, cur_char);

      break;
    }
    else
    {
      StyioAST* tmpStmt = parse_stmt(code, cur_char);
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

void parse_program (std::string styio_code) 
{
  int cur_char = styio_code.at(0);

  struct StyioCodeContext styio_code_context = {
    styio_code,
    0
  };

  StyioCodeContext* ctx_ptr = &styio_code_context;

  while (1) 
  {
    StyioAST* stmt = parse_stmt(ctx_ptr, cur_char);

    if ((stmt -> hint()) == StyioType::End) break;

    // fprintf(stderr, "[>_<] HERE!\n");

    std::cout << "\033[1;33m[>_<]\033[0m " << stmt -> toString() << std::endl;
  };
}