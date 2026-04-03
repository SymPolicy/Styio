#pragma once
#ifndef STYIO_COMPILATION_SESSION_HPP_
#define STYIO_COMPILATION_SESSION_HPP_

#include <utility>
#include <vector>

#include "../StyioAST/AST.hpp"
#include "../StyioIR/StyioIR.hpp"
#include "../StyioParser/Parser.hpp"
#include "../StyioToken/Token.hpp"

/**
 * Checkpoint C.1/C.2 shell:
 * Own compiler graph lifetimes in one place so each migration step can be merged safely.
 */
class CompilationSession
{
private:
  std::vector<StyioToken*> tokens_;
  StyioContext* context_ = nullptr;
  MainBlockAST* ast_ = nullptr;
  StyioIR* ir_ = nullptr;

  void clear_tokens() {
    for (auto* t : tokens_) {
      delete t;
    }
    tokens_.clear();
  }

  void clear_tracked_ast_nodes() {
    StyioAST::destroy_all_tracked_nodes();
  }

public:
  CompilationSession() = default;

  CompilationSession(const CompilationSession&) = delete;
  CompilationSession& operator=(const CompilationSession&) = delete;

  ~CompilationSession() {
    reset();
  }

  void reset() {
    if (ir_ != nullptr) {
      delete ir_;
      ir_ = nullptr;
    }
    if (context_ != nullptr) {
      delete context_;
      context_ = nullptr;
    }
    if (ast_ != nullptr) {
      delete ast_;
      ast_ = nullptr;
    }
    clear_tracked_ast_nodes();
    clear_tokens();
  }

  void adopt_tokens(std::vector<StyioToken*>&& tokens) {
    clear_tokens();
    tokens_ = std::move(tokens);
  }

  std::vector<StyioToken*>& tokens() {
    return tokens_;
  }

  const std::vector<StyioToken*>& tokens() const {
    return tokens_;
  }

  StyioContext* attach_context(StyioContext* ctx) {
    if (context_ != nullptr) {
      delete context_;
    }
    context_ = ctx;
    return context_;
  }

  StyioContext* context() {
    return context_;
  }

  MainBlockAST* attach_ast(MainBlockAST* ast) {
    if (ast_ != nullptr && ast_ != ast) {
      delete ast_;
    }
    ast_ = ast;
    return ast_;
  }

  MainBlockAST* ast() {
    return ast_;
  }

  StyioIR* attach_ir(StyioIR* ir) {
    if (ir_ != nullptr) {
      delete ir_;
    }
    ir_ = ir;
    return ir_;
  }

  StyioIR* ir() {
    return ir_;
  }
};

#endif // STYIO_COMPILATION_SESSION_HPP_
