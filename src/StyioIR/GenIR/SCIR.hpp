#pragma once
#ifndef STYIO_SC_IR_H_
#define STYIO_SC_IR_H_

#include "SGIR.hpp"

/*
  SC = Styio Collection. List, dictionary, and future collection/data
  structure nodes such as matrix live here.
*/

class SCListLiteral : public StyioIRTraits<SCListLiteral>
{
public:
  std::vector<StyioIR*> elems;
  std::string elem_type = "i64";

  SCListLiteral(std::vector<StyioIR*> e, std::string et) :
      elems(std::move(e)), elem_type(std::move(et)) {
  }

  static SCListLiteral* Create(std::vector<StyioIR*> e, std::string elem_type = "i64") {
    return new SCListLiteral(std::move(e), std::move(elem_type));
  }
};

class SCDictLiteral : public StyioIRTraits<SCDictLiteral>
{
public:
  struct Entry
  {
    StyioIR* key = nullptr;
    StyioIR* value = nullptr;
  };

  std::vector<Entry> entries;
  std::string value_type = "i64";

  SCDictLiteral(std::vector<Entry> e, std::string vt) :
      entries(std::move(e)), value_type(std::move(vt)) {
  }

  static SCDictLiteral* Create(std::vector<Entry> e, std::string value_type = "i64") {
    return new SCDictLiteral(std::move(e), std::move(value_type));
  }
};

class SCMatrixLiteral : public StyioIRTraits<SCMatrixLiteral>
{
public:
  std::vector<StyioIR*> elems;
  std::string elem_type = "i64";
  size_t rows = 0;
  size_t cols = 0;

  SCMatrixLiteral(std::vector<StyioIR*> e, std::string et, size_t r, size_t c) :
      elems(std::move(e)), elem_type(std::move(et)), rows(r), cols(c) {
  }

  static SCMatrixLiteral* Create(
    std::vector<StyioIR*> e,
    std::string elem_type,
    size_t rows,
    size_t cols
  ) {
    return new SCMatrixLiteral(std::move(e), std::move(elem_type), rows, cols);
  }
};

class SCListClone : public StyioIRTraits<SCListClone>
{
public:
  StyioIR* source = nullptr;

  explicit SCListClone(StyioIR* src) :
      source(src) {
  }

  static SCListClone* Create(StyioIR* src) {
    return new SCListClone(src);
  }
};

class SCListLen : public StyioIRTraits<SCListLen>
{
public:
  StyioIR* list = nullptr;

  explicit SCListLen(StyioIR* l) :
      list(l) {
  }

  static SCListLen* Create(StyioIR* l) {
    return new SCListLen(l);
  }
};

class SCListGet : public StyioIRTraits<SCListGet>
{
public:
  StyioIR* list = nullptr;
  StyioIR* index = nullptr;
  std::string elem_type = "i64";

  SCListGet(StyioIR* l, StyioIR* idx, std::string elem = "i64") :
      list(l), index(idx), elem_type(std::move(elem)) {
  }

  static SCListGet* Create(StyioIR* l, StyioIR* idx, std::string elem = "i64") {
    return new SCListGet(l, idx, std::move(elem));
  }
};

class SCListSet : public StyioIRTraits<SCListSet>
{
public:
  StyioIR* list = nullptr;
  StyioIR* index = nullptr;
  StyioIR* value = nullptr;
  std::string elem_type = "i64";

  SCListSet(StyioIR* l, StyioIR* idx, StyioIR* v, std::string elem = "i64") :
      list(l), index(idx), value(v), elem_type(std::move(elem)) {
  }

  static SCListSet* Create(StyioIR* l, StyioIR* idx, StyioIR* v, std::string elem_type = "i64") {
    return new SCListSet(l, idx, v, std::move(elem_type));
  }
};

class SCListToString : public StyioIRTraits<SCListToString>
{
public:
  StyioIR* list = nullptr;

  explicit SCListToString(StyioIR* l) :
      list(l) {
  }

  static SCListToString* Create(StyioIR* l) {
    return new SCListToString(l);
  }
};

class SCMatrixGet : public StyioIRTraits<SCMatrixGet>
{
public:
  StyioIR* matrix = nullptr;
  StyioIR* row = nullptr;
  StyioIR* col = nullptr;
  std::string elem_type = "i64";

  SCMatrixGet(StyioIR* m, StyioIR* r, StyioIR* c, std::string et) :
      matrix(m), row(r), col(c), elem_type(std::move(et)) {
  }

  static SCMatrixGet* Create(StyioIR* m, StyioIR* r, StyioIR* c, std::string elem_type) {
    return new SCMatrixGet(m, r, c, std::move(elem_type));
  }
};

class SCMatrixRow : public StyioIRTraits<SCMatrixRow>
{
public:
  StyioIR* matrix = nullptr;
  StyioIR* row = nullptr;
  std::string elem_type = "i64";

  SCMatrixRow(StyioIR* m, StyioIR* r, std::string et) :
      matrix(m), row(r), elem_type(std::move(et)) {
  }

  static SCMatrixRow* Create(StyioIR* m, StyioIR* r, std::string elem_type) {
    return new SCMatrixRow(m, r, std::move(elem_type));
  }
};

class SCMatrixToString : public StyioIRTraits<SCMatrixToString>
{
public:
  StyioIR* matrix = nullptr;

  explicit SCMatrixToString(StyioIR* m) :
      matrix(m) {
  }

  static SCMatrixToString* Create(StyioIR* m) {
    return new SCMatrixToString(m);
  }
};

class SCDictClone : public StyioIRTraits<SCDictClone>
{
public:
  StyioIR* source = nullptr;

  explicit SCDictClone(StyioIR* src) :
      source(src) {
  }

  static SCDictClone* Create(StyioIR* src) {
    return new SCDictClone(src);
  }
};

class SCDictLen : public StyioIRTraits<SCDictLen>
{
public:
  StyioIR* dict = nullptr;

  explicit SCDictLen(StyioIR* d) :
      dict(d) {
  }

  static SCDictLen* Create(StyioIR* d) {
    return new SCDictLen(d);
  }
};

class SCDictGet : public StyioIRTraits<SCDictGet>
{
public:
  StyioIR* dict = nullptr;
  StyioIR* key = nullptr;
  std::string value_type = "i64";

  SCDictGet(StyioIR* d, StyioIR* k, std::string vt) :
      dict(d), key(k), value_type(std::move(vt)) {
  }

  static SCDictGet* Create(StyioIR* d, StyioIR* k, std::string value_type = "i64") {
    return new SCDictGet(d, k, std::move(value_type));
  }
};

class SCDictSet : public StyioIRTraits<SCDictSet>
{
public:
  StyioIR* dict = nullptr;
  StyioIR* key = nullptr;
  StyioIR* value = nullptr;
  std::string value_type = "i64";

  SCDictSet(StyioIR* d, StyioIR* k, StyioIR* v, std::string vt) :
      dict(d), key(k), value(v), value_type(std::move(vt)) {
  }

  static SCDictSet* Create(StyioIR* d, StyioIR* k, StyioIR* v, std::string value_type = "i64") {
    return new SCDictSet(d, k, v, std::move(value_type));
  }
};

class SCDictKeys : public StyioIRTraits<SCDictKeys>
{
public:
  StyioIR* dict = nullptr;

  explicit SCDictKeys(StyioIR* d) :
      dict(d) {
  }

  static SCDictKeys* Create(StyioIR* d) {
    return new SCDictKeys(d);
  }
};

class SCDictValues : public StyioIRTraits<SCDictValues>
{
public:
  StyioIR* dict = nullptr;
  std::string value_type = "i64";

  explicit SCDictValues(StyioIR* d, std::string vt) :
      dict(d), value_type(std::move(vt)) {
  }

  static SCDictValues* Create(StyioIR* d, std::string value_type = "i64") {
    return new SCDictValues(d, std::move(value_type));
  }
};

class SCDictToString : public StyioIRTraits<SCDictToString>
{
public:
  StyioIR* dict = nullptr;

  explicit SCDictToString(StyioIR* d) :
      dict(d) {
  }

  static SCDictToString* Create(StyioIR* d) {
    return new SCDictToString(d);
  }
};

#endif
