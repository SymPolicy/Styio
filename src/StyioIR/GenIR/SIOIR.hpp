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

  ~SIOHandleAcquire() override {
    delete path_expr;
  }

  static SIOHandleAcquire* Create(std::string v, StyioIR* p, bool a) {
    return new SIOHandleAcquire(std::move(v), p, a);
  }
};

class SIOHandleRelease : public StyioIRTraits<SIOHandleRelease>
{
public:
  std::string var_name;
  StyioIR* path_expr = nullptr;
  bool is_auto = false;
  bool from_path = false;

  static SIOHandleRelease* CreateFromVar(std::string v) {
    auto* x = new SIOHandleRelease();
    x->var_name = std::move(v);
    x->from_path = false;
    return x;
  }

  static SIOHandleRelease* CreateFromPath(StyioIR* p, bool a) {
    auto* x = new SIOHandleRelease();
    x->path_expr = p;
    x->is_auto = a;
    x->from_path = true;
    return x;
  }

  ~SIOHandleRelease() override {
    delete path_expr;
  }

private:
  SIOHandleRelease() = default;
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

  ~SIOFileLineIter() override {
    delete path_expr;
    delete body;
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
  std::string a_elem_type = "i64";
  std::string b_elem_type = "i64";
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
    std::string a_elem,
    std::string b_elem,
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
    z->a_elem_type = std::move(a_elem);
    z->b_elem_type = std::move(b_elem);
    z->body = b;
    return z;
  }

  void set_pulse_plan(std::unique_ptr<SGPulsePlan> p) {
    pulse_plan = std::move(p);
  }

  ~SIOStreamZip() override {
    delete iterable_a;
    delete iterable_b;
    delete body;
  }
};

class SIOInstantPull : public StyioIRTraits<SIOInstantPull>
{
public:
  StyioIR* path_expr = nullptr;

  explicit SIOInstantPull(StyioIR* p) :
      path_expr(p) {
  }

  ~SIOInstantPull() override {
    delete path_expr;
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
  std::string required_handle_var;

  static SIOResourceWriteToFile* Create(
    StyioIR* d,
    StyioIR* p,
    bool auto_p,
    bool prom,
    bool append_nl = false,
    std::string required_handle = ""
  ) {
    auto* x = new SIOResourceWriteToFile();
    x->data_expr = d;
    x->path_expr = p;
    x->is_auto_path = auto_p;
    x->promote_data_to_cstr = prom;
    x->append_newline = append_nl;
    x->required_handle_var = std::move(required_handle);
    return x;
  }

  ~SIOResourceWriteToFile() override {
    delete data_expr;
    delete path_expr;
  }

private:
  SIOResourceWriteToFile() = default;
};

/* Standard streams: write to stdout / stderr */
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

  ~SIOStdStreamWrite() override {
    styio_delete_ir_nodes(exprs);
  }

private:
  SIOStdStreamWrite() = default;
};

class SIOResourceEffect : public StyioIRTraits<SIOResourceEffect>
{
public:
  struct Handler {
    std::string effect_name;
    StyioIR* body = nullptr;

    Handler(std::string effect, StyioIR* handler_body) :
        effect_name(std::move(effect)), body(handler_body) {
    }
  };

  StyioIR* operation = nullptr;
  StyioIR* fallback = nullptr;
  std::vector<Handler> handlers;
  bool discard = false;
  bool value_required = false;
  StyioDataType result_type{StyioDataTypeOption::Undefined, "undefined", 0};

  static SIOResourceEffect* Create(
    StyioIR* op,
    StyioIR* fb,
    bool discard_effect,
    StyioDataType result,
    std::vector<Handler> effect_handlers = {},
    bool requires_value = false
  ) {
    auto* x = new SIOResourceEffect();
    x->operation = op;
    x->fallback = fb;
    x->handlers = std::move(effect_handlers);
    x->discard = discard_effect;
    x->value_required = requires_value;
    x->result_type = std::move(result);
    return x;
  }

  ~SIOResourceEffect() override {
    delete operation;
    delete fallback;
    for (auto& handler : handlers) {
      delete handler.body;
    }
  }

private:
  SIOResourceEffect() = default;
};

/* Stdio input: read lines from stdin */
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

  ~SIOStdStreamLineIter() override {
    delete body;
  }

private:
  SIOStdStreamLineIter() = default;
};

/* Stdio input: single-read pull from stdin */
class SIOStdStreamPull : public StyioIRTraits<SIOStdStreamPull>
{
public:
  StyioDataType result_type{StyioDataTypeOption::Integer, "i64", 64};

  static SIOStdStreamPull* Create() {
    return new SIOStdStreamPull();
  }

  static SIOStdStreamPull* Create(StyioDataType result_type) {
    return new SIOStdStreamPull(std::move(result_type));
  }

  SIOStdStreamPull() = default;

  explicit SIOStdStreamPull(StyioDataType result_type) :
      result_type(std::move(result_type)) {
  }
};

class SIOTaskCreate : public StyioIRTraits<SIOTaskCreate>
{
public:
  SGBlock* body = nullptr;
  StyioDataType result_type{StyioDataTypeOption::Integer, "i64", 64};

  static SIOTaskCreate* Create(SGBlock* b, StyioDataType result) {
    auto* x = new SIOTaskCreate();
    x->body = b;
    x->result_type = std::move(result);
    return x;
  }

  ~SIOTaskCreate() override {
    delete body;
  }

private:
  SIOTaskCreate() = default;
};

class SIOFlowBind : public StyioIRTraits<SIOFlowBind>
{
public:
  StyioIR* source_expr = nullptr;
  StyioIR* fallback_expr = nullptr;
  std::string target_name;
  StyioDataType result_type{StyioDataTypeOption::Integer, "i64", 64};
  bool source_is_task = false;
  bool await_bind = false;

  static SIOFlowBind* Create(
    StyioIR* source,
    std::string target,
    StyioDataType result,
    bool task_source,
    StyioIR* fallback = nullptr,
    bool is_await = false
  ) {
    auto* x = new SIOFlowBind();
    x->source_expr = source;
    x->fallback_expr = fallback;
    x->target_name = std::move(target);
    x->result_type = std::move(result);
    x->source_is_task = task_source;
    x->await_bind = is_await;
    return x;
  }

  ~SIOFlowBind() override {
    delete source_expr;
    delete fallback_expr;
  }

private:
  SIOFlowBind() = default;
};

/*
  The classes below (SIOPath, SIOPrint, SIORead) were merged from the
  former `StyioIR/IOIR/IOIR.hpp` during the 2026-05-22 history-burden
  reduction pass. They share the SIO domain prefix and live here so the
  IO/std-stream node family stays in a single header.
*/

class SIOPath : public StyioIRTraits<SIOPath>
{
public:
  std::string path;

  SIOPath(std::string path) :
      path(path) {
  }

  static SIOPath* Create(std::string path) {
    return new SIOPath(path);
  }
};

class SIOPrint : public StyioIRTraits<SIOPrint>
{
public:
  std::vector<StyioIR*> expr;

  SIOPrint(std::vector<StyioIR*> expr) :
      expr(expr) {
  }

  ~SIOPrint() override {
    styio_delete_ir_nodes(expr);
  }

  static SIOPrint* Create(std::vector<StyioIR*> expr) {
    return new SIOPrint(expr);
  }
};

class SIORead : public StyioIRTraits<SIORead>
{
public:
  SIOPath* file_path;

  SIORead(SIOPath* file_path) :
      file_path(file_path) {
  }

  ~SIORead() override {
    delete file_path;
  }

  static SIORead* Create(SIOPath* file_path) {
    return new SIORead(file_path);
  }
};

#endif
