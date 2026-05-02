#pragma once
#ifndef STYIO_SIO_IR_H_
#define STYIO_SIO_IR_H_

#include "SGIR.hpp"

/*
  SIO = Styio Input/Output. Files, standard streams, stdin/stdout/stderr,
  future network, and filesystem IO nodes live here.
*/

class SIOHandleAcquire : public StyioIRTraits<SIOHandleAcquire>
{
public:
  std::string var_name;
  StyioIR* path_expr = nullptr;
  bool is_auto = false;

  SIOHandleAcquire(std::string v, StyioIR* p, bool a) :
      var_name(std::move(v)), path_expr(p), is_auto(a) {
  }

  static SIOHandleAcquire* Create(std::string v, StyioIR* p, bool a) {
    return new SIOHandleAcquire(std::move(v), p, a);
  }
};

class SIOFileLineIter : public StyioIRTraits<SIOFileLineIter>
{
public:
  bool from_path = true;
  StyioIR* path_expr = nullptr;
  std::string handle_var;
  std::string line_var;
  SGBlock* body = nullptr;
  std::unique_ptr<SGPulsePlan> pulse_plan;
  int pulse_region_id = -1;

  static SIOFileLineIter* CreateFromPath(StyioIR* path, std::string line, SGBlock* b) {
    auto* r = new SIOFileLineIter();
    r->from_path = true;
    r->path_expr = path;
    r->line_var = std::move(line);
    r->body = b;
    return r;
  }

  static SIOFileLineIter* CreateFromHandle(std::string hvar, std::string line, SGBlock* b) {
    auto* r = new SIOFileLineIter();
    r->from_path = false;
    r->handle_var = std::move(hvar);
    r->line_var = std::move(line);
    r->body = b;
    return r;
  }

  void set_pulse_plan(std::unique_ptr<SGPulsePlan> p) {
    pulse_plan = std::move(p);
  }

private:
  SIOFileLineIter() = default;
};

class SIOStreamZip : public StyioIRTraits<SIOStreamZip>
{
public:
  StyioIR* iterable_a = nullptr;
  bool a_is_file = false;
  std::string var_a;
  StyioIR* iterable_b = nullptr;
  bool b_is_file = false;
  std::string var_b;
  bool a_elem_string = false;
  bool b_elem_string = false;
  SGBlock* body = nullptr;
  std::unique_ptr<SGPulsePlan> pulse_plan;
  int pulse_region_id = -1;

  static SIOStreamZip* Create(
    StyioIR* ia,
    bool fa,
    std::string va,
    StyioIR* ib,
    bool fb,
    std::string vb,
    bool astr,
    bool bstr,
    SGBlock* b
  ) {
    auto* z = new SIOStreamZip();
    z->iterable_a = ia;
    z->a_is_file = fa;
    z->var_a = std::move(va);
    z->iterable_b = ib;
    z->b_is_file = fb;
    z->var_b = std::move(vb);
    z->a_elem_string = astr;
    z->b_elem_string = bstr;
    z->body = b;
    return z;
  }

  void set_pulse_plan(std::unique_ptr<SGPulsePlan> p) {
    pulse_plan = std::move(p);
  }
};

class SIOInstantPull : public StyioIRTraits<SIOInstantPull>
{
public:
  StyioIR* path_expr = nullptr;

  explicit SIOInstantPull(StyioIR* p) :
      path_expr(p) {
  }

  static SIOInstantPull* Create(StyioIR* p) {
    return new SIOInstantPull(p);
  }
};

class SIOListReadStdin : public StyioIRTraits<SIOListReadStdin>
{
public:
  std::string elem_type;

  explicit SIOListReadStdin(std::string elem) :
      elem_type(std::move(elem)) {
  }

  static SIOListReadStdin* Create(std::string elem_type) {
    return new SIOListReadStdin(std::move(elem_type));
  }
};

class SIOResourceWriteToFile : public StyioIRTraits<SIOResourceWriteToFile>
{
public:
  StyioIR* data_expr = nullptr;
  StyioIR* path_expr = nullptr;
  bool is_auto_path = false;
  bool promote_data_to_cstr = false;
  bool append_newline = false;

  static SIOResourceWriteToFile* Create(
    StyioIR* d,
    StyioIR* p,
    bool auto_p,
    bool prom,
    bool append_nl = false
  ) {
    auto* x = new SIOResourceWriteToFile();
    x->data_expr = d;
    x->path_expr = p;
    x->is_auto_path = auto_p;
    x->promote_data_to_cstr = prom;
    x->append_newline = append_nl;
    return x;
  }

private:
  SIOResourceWriteToFile() = default;
};

/* M9: write to stdout / stderr */
class SIOStdStreamWrite : public StyioIRTraits<SIOStdStreamWrite>
{
public:
  enum class Stream { Stdout, Stderr };

  Stream stream = Stream::Stdout;
  std::vector<StyioIR*> exprs;

  static SIOStdStreamWrite* Create(Stream s, std::vector<StyioIR*> e) {
    auto* x = new SIOStdStreamWrite();
    x->stream = s;
    x->exprs = std::move(e);
    return x;
  }

private:
  SIOStdStreamWrite() = default;
};

/* M10: read lines from stdin */
class SIOStdStreamLineIter : public StyioIRTraits<SIOStdStreamLineIter>
{
public:
  std::string line_var;
  SGBlock* body = nullptr;
  std::unique_ptr<SGPulsePlan> pulse_plan;
  int pulse_region_id = -1;

  static SIOStdStreamLineIter* Create(std::string line, SGBlock* b) {
    auto* r = new SIOStdStreamLineIter();
    r->line_var = std::move(line);
    r->body = b;
    return r;
  }

  void set_pulse_plan(std::unique_ptr<SGPulsePlan> p) {
    pulse_plan = std::move(p);
  }

private:
  SIOStdStreamLineIter() = default;
};

/* M10: single-read pull from stdin */
class SIOStdStreamPull : public StyioIRTraits<SIOStdStreamPull>
{
public:
  static SIOStdStreamPull* Create() {
    return new SIOStdStreamPull();
  }

private:
  SIOStdStreamPull() = default;
};

#endif
