#pragma once
#ifndef STYIO_BINOP_MAPPER_H_
#define STYIO_BINOP_MAPPER_H_

// [C++ Standard]
#include <functional>

// [Styio]
#include "../StyioAST/AST.hpp"

StyioAST* binop_addition(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(StyioOpType::Binary_Add, left, right);
};

StyioAST* binop_subtraction(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(StyioOpType::Binary_Sub, left, right);
};

BinOpAST* binop_multiplication(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(StyioOpType::Binary_Mul, left, right);
};


StyioAST* binop_division(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(StyioOpType::Binary_Div, left, right);
};

StyioAST* binop_modulo(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(StyioOpType::Binary_Mod, left, right);
};

StyioAST* binop_power(StyioAST* left, StyioAST* right) {
  return BinOpAST::Create(StyioOpType::Binary_Pow, left, right);
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
std::unordered_map<StyioOpType, bin_op_func> bin_op_mapper{
  {StyioOpType::Binary_Add, binop_addition},
  {StyioOpType::Binary_Sub, binop_subtraction},
  {StyioOpType::Binary_Mul, binop_multiplication},
  {StyioOpType::Binary_Div, binop_division},

  {StyioOpType::Binary_Mod, binop_modulo},
  {StyioOpType::Binary_Pow, binop_power},

  {StyioOpType::Logic_NOT, cond_not},
  {StyioOpType::Logic_AND, cond_and},
  {StyioOpType::Logic_OR, cond_or},
  {StyioOpType::Logic_XOR, cond_xor}
};

#endif