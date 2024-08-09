#pragma once
#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

StyioAST* binop_addition(StyioAST* left, StyioAST* right) {
  
};

using bin_op_func = std::function<StyioAST*(StyioAST*, StyioAST*)>;
std::unordered_map<std::string, bin_op_func> bin_op_mapper{
  // Plus function from functools
  {"+", std::plus<long>()},
  // Minus function from functools
  {"-", minus<long>()},
  // Divides function from functools
  {"/", divides<long>()},
  // Multiplies function from functools
  {"*", multiplies<long>()}
};

#endif