#pragma once
#ifndef STYIO_BINOP_MAPPER_H_
#define STYIO_BINOP_MAPPER_H_

// [C++ Standard]
#include <functional>

// [Styio]
#include "../StyioAST/AST.hpp"

StyioAST* binop_addition(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(TokenKind::Binary_Add, left, right);
};

StyioAST* binop_subtraction(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(TokenKind::Binary_Sub, left, right);
};

StyioAST* binop_multiplication(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(TokenKind::Binary_Mul, left, right);
};


StyioAST* binop_division(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(TokenKind::Binary_Div, left, right);
};

StyioAST* binop_modulo(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(TokenKind::Binary_Mod, left, right);
};

StyioAST* binop_power(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(TokenKind::Binary_Pow, left, right);
};

StyioAST* cond_not(StyioAST* left, StyioAST* right) {
  return CondAST::Create(LogicType::NOT, left);
};

StyioAST* cond_and(StyioAST* left, StyioAST* right) {
  return CondAST::Create(LogicType::AND, left, right);
};

StyioAST* cond_or(StyioAST* left, StyioAST* right) {
  return CondAST::Create(LogicType::OR, left, right);
};

StyioAST* cond_xor(StyioAST* left, StyioAST* right) {
  return CondAST::Create(LogicType::XOR, left, right);
};

using bin_op_func = std::function<StyioAST*(StyioAST*, StyioAST*)>;
std::unordered_map<TokenKind, bin_op_func> bin_op_mapper{
  {TokenKind::Binary_Add, binop_addition},
  {TokenKind::Binary_Sub, binop_subtraction},
  {TokenKind::Binary_Mul, binop_multiplication},
  {TokenKind::Binary_Div, binop_division},

  {TokenKind::Binary_Mod, binop_modulo},
  {TokenKind::Binary_Pow, binop_power},

  {TokenKind::Logic_NOT, cond_not},
  {TokenKind::Logic_AND, cond_and},
  {TokenKind::Logic_OR, cond_or},
  {TokenKind::Logic_XOR, cond_xor}
};

#endif