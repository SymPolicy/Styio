
#include <string>
#include <vector>
#include <iostream>

#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"
#include "Parser.hpp"

IdAST* parse_id (std::vector<int>& tok_ctx, int& cur_char)
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