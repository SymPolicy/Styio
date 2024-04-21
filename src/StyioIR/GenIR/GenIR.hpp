#pragma once
#ifndef STYIO_GENERAL_IR_H_
#define STYIO_GENERAL_IR_H_

// [C++ STL]
#include <string>
#include <vector>

// [Styio]
#include "../IRDecl.hpp"
#include "../StyioIR.hpp"
#include "../StyioToken/Token.hpp"

/*
  Styio General Intermediate Representation (Styio GenIR)

  GenIR should be directly converted to LLVM IR.
*/

/*
  SResId (Styio Resource Indentifier):
    The resource indentifier in Styio.
    SResId must be UNIQUE.
*/
class SGResId : public StyioIRTraits<SGResId>
{
private:
  SGResId(std::string id) :
      id(id) {
  }

public:
  std::string id;

  static SGResId* Create() {
    return new SGResId("");
  }

  static SGResId* Create(std::string id) {
    return new SGResId(id);
  }
};

class SGType : public StyioIRTraits<SGType>
{
private:
  SGType(StyioDataType data_type) :
      data_type(data_type) {
  }

public:
  StyioDataType data_type = StyioDataType::undefined;

  static SGType* Create() {
    return new SGType(StyioDataType::undefined);
  }

  static SGType* Create(StyioDataType data_type) {
    return new SGType(data_type);
  }

  static SGType* Create(std::string type_name) {
    auto it = DType_Table.find(type_name);
    if (it != DType_Table.end())
      return new SGType(it->second);
    else
      return new SGType(StyioDataType::undefined);
  }
};

class SGConstBool : public StyioIRTraits<SGConstBool>
{
private:
  SGConstBool(bool value) :
      value(value) {
  }

public:
  bool value;

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
private:
  SGConstInt(
    std::string value,
    size_t numbits
  ) :
      value(value),
      numbits(numbits) {
  }

public:
  std::string value;
  size_t numbits;

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
private:
  SGConstFloat(std::string value) :
      value(value) {
  }

public:
  std::string value;

  static SGConstFloat* Create(std::string value) {
    return new SGConstFloat(value);
  }
};

class SGConstChar : public StyioIRTraits<SGConstChar>
{
private:
  SGConstChar(char value) :
      value(value) {
  }

public:
  char value;

  static SGConstChar* Create(char value) {
    return new SGConstChar(value);
  }
};

class SGConstString : public StyioIRTraits<SGConstString>
{
private:
  SGConstString(std::string value) :
      value(value) {
  }

public:
  std::string value;

  static SGConstString* Create(std::string value) {
    return new SGConstString(value);
  }
};

class SGFormatString : public StyioIRTraits<SGFormatString>
{
private:
  SGFormatString(vector<string> fragments, vector<StyioAST*> expressions) :
      frags(std::move(fragments)), exprs(std::move(expressions)) {
  }

public:
  vector<string> frags;    /* fragments */
  vector<StyioAST*> exprs; /* expressions */

  static SGFormatString* Create(vector<string> fragments, vector<StyioAST*> expressions) {
    return new SGFormatString(fragments, expressions);
  };
};

/* Binary Operation Expression */
class SGBinOp : public StyioIRTraits<SGBinOp>
{
private:
  SGBinOp(StyioIR* lhs, StyioIR* rhs, TokenKind op, SGType* data_type) :
      lhs_expr(std::move(lhs)), rhs_expr(std::move(rhs)), operand(op), data_type(std::move(data_type)) {
  }

public:
  SGType* data_type;
  StyioIR* lhs_expr;
  StyioIR* rhs_expr;
  TokenKind operand;

  static SGBinOp* Create(StyioIR* lhs, StyioIR* rhs, TokenKind op, SGType* data_type) {
    return new SGBinOp(lhs, rhs, op, data_type);
  }
};

class SGFuncArg : public StyioIRTraits<SGFuncArg>
{
private:
  SGFuncArg(std::string id, SGType* type) :
      id(id), arg_type(type) {
  }

public:
  std::string id;
  SGType* arg_type;

  static SGFuncArg* Create(std::string id, SGType* type) {
    return new SGFuncArg(id, type);
  }
};

class SGFunc : StyioIRTraits<SGFunc>
{
private:
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

public:
  SGType* ret_type;
  SGResId* func_name;
  std::vector<SGFuncArg*> func_args;
  SGBlock* func_block;

  static SGFunc* Create(SGType* ret_type, SGResId* func_name, std::vector<SGFuncArg*> func_args, SGBlock* func_block) {
    return new SGFunc(ret_type, func_name, func_args, func_block);
  }
};

class SGCall : StyioIRTraits<SGCall>
{
private:
  SGCall(SGResId* func_name, std::vector<StyioIR*> func_args) :
      func_name(std::move(func_name)), func_args(std::move(func_args)) {
  }

public:
  SGResId* func_name;
  std::vector<StyioIR*> func_args;

  static SGCall* Create(SGResId* func_name, std::vector<StyioIR*> func_args) {
    return new SGCall(std::move(func_name), std::move(func_args));
  }
};

class SGReturn : StyioIRTraits<SGReturn>
{
private:
  SGReturn(StyioIR* expr) :
      expr(expr) {
  }

public:
  StyioIR* expr;

  static SGReturn* Create(StyioIR* expr) {
    return new SGReturn(expr);
  }
};

class SGBlock : StyioIRTraits<SGBlock>
{
private:
  SGBlock(std::vector<StyioIR*> stmts) :
      stmts(std::move(stmts)) {
  }

public:
  std::vector<StyioIR*> stmts;

  static SGBlock* Create(std::vector<StyioIR*> stmts) {
    return new SGBlock(std::move(stmts));
  }
};

class SGMainEntry : StyioIRTraits<SGMainEntry>
{
private:
  SGMainEntry(std::vector<StyioIR*> stmts) :
      stmts(std::move(stmts)) {
  }

public:
  std::vector<StyioIR*> stmts;

  static SGMainEntry* Create(std::vector<StyioIR*> stmts) {
    return new SGMainEntry(std::move(stmts));
  }
};

#endif