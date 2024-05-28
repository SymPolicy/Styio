#pragma once
#ifndef STYIO_GENERAL_IR_H_
#define STYIO_GENERAL_IR_H_

// [C++ STL]
#include <string>
#include <vector>

// [Styio]
#include "../../StyioToken/Token.hpp"
#include "../IRDecl.hpp"
#include "../StyioIR.hpp"

/*
  Styio General Intermediate Representation (Styio GenIR)

  GenIR should be directly converted to LLVM IR.
*/

/*
  SResId (Styio Resource Indentifier)
*/
class SGResId : public StyioIRTraits<SGResId>
{
private:
  std::string id;

public:
  SGResId(std::string id) :
      id(id) {
  }

  static SGResId* Create() {
    return new SGResId("");
  }

  static SGResId* Create(std::string id) {
    return new SGResId(id);
  }

  const std::string& as_str() {
    return id;
  }
};

class SGType : public StyioIRTraits<SGType>
{
public:
  StyioDataType data_type;

  SGType(StyioDataType data_type) :
      data_type(data_type) {
  }

  static SGType* Create(StyioDataType data_type) {
    return new SGType(data_type);
  }
};

class SGConstBool : public StyioIRTraits<SGConstBool>
{
public:
  bool value;

  SGConstBool(bool value) :
      value(value) {
  }

  static SGConstBool* Create(bool value) {
    return new SGConstBool(value);
  }
};

/*
  Two Types of Numbers:
  - Constant Integer
  - Constant Float
*/
class SGConstInt : public StyioIRTraits<SGConstInt>
{
public:
  std::string value;
  size_t num_of_bit;

  SGConstInt(
    std::string value,
    size_t numbits
  ) :
      value(value),
      num_of_bit(numbits) {
  }

  static SGConstInt* Create(long value) {
    return new SGConstInt(std::to_string(value), 64);
  }

  static SGConstInt* Create(std::string value) {
    return new SGConstInt(value, 64);
  }

  static SGConstInt* Create(std::string value, size_t numbits) {
    return new SGConstInt(value, numbits);
  }
};

class SGConstFloat : public StyioIRTraits<SGConstFloat>
{
public:
  std::string value;

  SGConstFloat(std::string value) :
      value(value) {
  }

  static SGConstFloat* Create(std::string value) {
    return new SGConstFloat(value);
  }
};

class SGConstChar : public StyioIRTraits<SGConstChar>
{
public:
  char value;

  SGConstChar(char value) :
      value(value) {
  }

  static SGConstChar* Create(char value) {
    return new SGConstChar(value);
  }
};

class SGConstString : public StyioIRTraits<SGConstString>
{
public:
  std::string value;

  SGConstString(std::string value) :
      value(value) {
  }

  static SGConstString* Create(std::string value) {
    return new SGConstString(value);
  }
};

class SGFormatString : public StyioIRTraits<SGFormatString>
{
public:
  std::vector<string> frags;   /* fragments */
  std::vector<StyioIR*> exprs; /* expressions */

  SGFormatString(std::vector<string> fragments, std::vector<StyioIR*> expressions) :
      frags(std::move(fragments)), exprs(std::move(expressions)) {
  }

  static SGFormatString* Create(std::vector<string> fragments, std::vector<StyioIR*> expressions) {
    return new SGFormatString(fragments, expressions);
  };
};

class SGStruct : public StyioIRTraits<SGStruct>
{
public:
  SGResId* name;
  std::vector<SGVar*> elements;

  SGStruct(std::vector<SGVar*> elements) :
      elements(elements) {
  }

  SGStruct(SGResId* name, std::vector<SGVar*> elements) :
      name(name), elements(elements) {
  }

  static SGStruct* Create(std::vector<SGVar*> elements) {
    return new SGStruct(elements);
  }

  static SGStruct* Create(SGResId* name, std::vector<SGVar*> elements) {
    return new SGStruct(name, elements);
  }
};

class SGCast : public StyioIRTraits<SGCast>
{
public:
  SGType* from_type;
  SGType* to_type;

  SGCast(SGType* from_type, SGType* to_type) :
      from_type(from_type), to_type(to_type) {
  }

  static SGCast* Create(SGType* from_type, SGType* to_type) {
    return new SGCast(from_type, to_type);
  };
};

/* Binary Operation Expression */
class SGBinOp : public StyioIRTraits<SGBinOp>
{
public:
  SGType* data_type;
  StyioIR* lhs_expr;
  StyioIR* rhs_expr;
  TokenKind operand;

  SGBinOp(StyioIR* lhs, StyioIR* rhs, TokenKind op, SGType* data_type) :
      lhs_expr(std::move(lhs)), rhs_expr(std::move(rhs)), operand(op), data_type(std::move(data_type)) {
  }

  static SGBinOp* Create(StyioIR* lhs, StyioIR* rhs, TokenKind op, SGType* data_type) {
    return new SGBinOp(lhs, rhs, op, data_type);
  }
};

class SGCond : public StyioIRTraits<SGCond>
{
public:
  StyioIR* lhs_expr;
  StyioIR* rhs_expr;
  TokenKind operand;

  SGCond(StyioIR* lhs, StyioIR* rhs, TokenKind op) :
      lhs_expr(std::move(lhs)), rhs_expr(std::move(rhs)), operand(op) {
  }

  static SGCond* Create(StyioIR* lhs, StyioIR* rhs, TokenKind op) {
    return new SGCond(lhs, rhs, op);
  }
};

class SGVar : public StyioIRTraits<SGVar>
{
public:
  SGResId* var_name;
  SGType* var_type;
  StyioIR* val_init = nullptr;

  SGVar(SGResId* id, SGType* type) :
      var_name(id), var_type(type) {
  }

  SGVar(SGResId* id, SGType* type, StyioIR* value) :
      var_name(id), var_type(type), val_init(value) {
  }

  static SGVar* Create(SGResId* id, SGType* type) {
    return new SGVar(id, type);
  }

  static SGVar* Create(SGResId* id, SGType* type, StyioIR* value) {
    return new SGVar(id, type, value);
  }
};

class SGFlexBind : public StyioIRTraits<SGFlexBind>
{
public:
  SGVar* var;
  StyioIR* value;

  SGFlexBind(SGVar* var, StyioIR* value) :
      var(var), value(value) {
  }

  static SGFlexBind* Create(SGVar* id, StyioIR* value) {
    return new SGFlexBind(id, value);
  }
};

class SGFinalBind : public StyioIRTraits<SGFinalBind>
{
public:
  SGVar* var;
  StyioIR* value;

  SGFinalBind(SGVar* var, StyioIR* value) :
      var(var), value(value) {
  }

  static SGFinalBind* Create(SGVar* id, StyioIR* value) {
    return new SGFinalBind(id, value);
  }
};

class SGFuncArg : public StyioIRTraits<SGFuncArg>
{
public:
  std::string id;
  SGType* arg_type;

  SGFuncArg(std::string id, SGType* type) :
      id(id), arg_type(type) {
  }

  static SGFuncArg* Create(std::string id, SGType* type) {
    return new SGFuncArg(id, type);
  }
};

class SGFunc : public StyioIRTraits<SGFunc>
{
public:
  SGType* ret_type;
  SGResId* func_name;
  std::vector<SGFuncArg*> func_args;
  SGBlock* func_block;

  SGFunc(
    SGType* ret_type,
    SGResId* func_name,
    std::vector<SGFuncArg*> func_args,
    SGBlock* func_block
  ) :
      ret_type(std::move(ret_type)),
      func_name(std::move(func_name)),
      func_args(std::move(func_args)),
      func_block(std::move(func_block)) {
  }

  static SGFunc* Create(SGType* ret_type, SGResId* func_name, std::vector<SGFuncArg*> func_args, SGBlock* func_block) {
    return new SGFunc(ret_type, func_name, func_args, func_block);
  }
};

class SGCall : public StyioIRTraits<SGCall>
{
public:
  SGResId* func_name;
  std::vector<StyioIR*> func_args;

  SGCall(SGResId* func_name, std::vector<StyioIR*> func_args) :
      func_name(std::move(func_name)), func_args(std::move(func_args)) {
  }

  static SGCall* Create(SGResId* func_name, std::vector<StyioIR*> func_args) {
    return new SGCall(std::move(func_name), std::move(func_args));
  }
};

class SGReturn : public StyioIRTraits<SGReturn>
{
public:
  StyioIR* expr;

  SGReturn(StyioIR* expr) :
      expr(expr) {
  }

  static SGReturn* Create(StyioIR* expr) {
    return new SGReturn(expr);
  }
};

class SGBlock : public StyioIRTraits<SGBlock>
{
public:
  std::vector<StyioIR*> stmts;

  SGBlock(std::vector<StyioIR*> stmts) :
      stmts(std::move(stmts)) {
  }

  static SGBlock* Create(std::vector<StyioIR*> stmts) {
    return new SGBlock(std::move(stmts));
  }
};

class SGEntry : public StyioIRTraits<SGEntry>
{
public:
  std::vector<StyioIR*> stmts;

  SGEntry(std::vector<StyioIR*> stmts) :
      stmts(std::move(stmts)) {
  }

  static SGEntry* Create(std::vector<StyioIR*> stmts) {
    return new SGEntry(std::move(stmts));
  }
};

class SGMainEntry : public StyioIRTraits<SGMainEntry>
{
public:
  std::vector<StyioIR*> stmts;

  SGMainEntry(std::vector<StyioIR*> stmts) :
      stmts(stmts) {
  }

  static SGMainEntry* Create(std::vector<StyioIR*> stmts) {
    return new SGMainEntry(stmts);
  }
};

#endif