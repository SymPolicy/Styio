#pragma once
#ifndef STYIO_COMPILATION_SESSION_HPP_
#define STYIO_COMPILATION_SESSION_HPP_

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../StyioAST/AST.hpp"
#include "../StyioIR/StyioIR.hpp"
#include "../StyioParser/Parser.hpp"
#include "SessionAllocation.hpp"
#include "../StyioToken/Token.hpp"

enum class CompilationPhase
{
  Empty,
  Tokenized,
  Parsed,
  Typed,
  Lowered,
  CodegenReady,
  Executed,
  Failed,
};

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
  styio::session_alloc::SessionArena ast_arena_;
  styio::session_alloc::SessionArena token_arena_;
  styio::session_alloc::SessionArena* previous_ast_arena_ = nullptr;
  styio::session_alloc::SessionArena* previous_token_arena_ = nullptr;
  CompilationPhase phase_ = CompilationPhase::Empty;

  static int phase_rank(CompilationPhase phase) {
    switch (phase) {
      case CompilationPhase::Empty:
        return 0;
      case CompilationPhase::Tokenized:
        return 1;
      case CompilationPhase::Parsed:
        return 2;
      case CompilationPhase::Typed:
        return 3;
      case CompilationPhase::Lowered:
        return 4;
      case CompilationPhase::CodegenReady:
        return 5;
      case CompilationPhase::Executed:
        return 6;
      case CompilationPhase::Failed:
        return 7;
    }
    return 0;
  }

  static const char* phase_name(CompilationPhase phase) {
    switch (phase) {
      case CompilationPhase::Empty:
        return "Empty";
      case CompilationPhase::Tokenized:
        return "Tokenized";
      case CompilationPhase::Parsed:
        return "Parsed";
      case CompilationPhase::Typed:
        return "Typed";
      case CompilationPhase::Lowered:
        return "Lowered";
      case CompilationPhase::CodegenReady:
        return "CodegenReady";
      case CompilationPhase::Executed:
        return "Executed";
      case CompilationPhase::Failed:
        return "Failed";
    }
    return "Unknown";
  }

  static bool can_transition(CompilationPhase current, CompilationPhase next) {
    if (current == next) {
      return true;
    }
    if (next == CompilationPhase::Failed) {
      return current != CompilationPhase::Empty;
    }
    if (current == CompilationPhase::Failed) {
      return false;
    }
    return phase_rank(next) >= phase_rank(current);
  }

  void transition_to(CompilationPhase next, const char* operation) {
    if (!can_transition(phase_, next)) {
      throw std::logic_error(
        std::string("invalid compilation session transition in ")
        + operation
        + ": "
        + phase_name(phase_)
        + " -> "
        + phase_name(next));
    }
    phase_ = next;
  }

  void activate_session_arenas() {
    previous_ast_arena_ = styio::session_alloc::set_current_ast_arena(&ast_arena_);
    previous_token_arena_ = styio::session_alloc::set_current_token_arena(&token_arena_);
  }

  void deactivate_session_arenas() {
    styio::session_alloc::set_current_ast_arena(previous_ast_arena_);
    styio::session_alloc::set_current_token_arena(previous_token_arena_);
    previous_ast_arena_ = nullptr;
    previous_token_arena_ = nullptr;
  }

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
  CompilationSession() :
      ast_arena_(256u * 1024u),
      token_arena_(64u * 1024u) {
    activate_session_arenas();
  }

  CompilationSession(const CompilationSession&) = delete;
  CompilationSession& operator=(const CompilationSession&) = delete;

  ~CompilationSession() {
    reset();
    deactivate_session_arenas();
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
    ast_arena_.release();
    token_arena_.release();
    phase_ = CompilationPhase::Empty;
  }

  void adopt_tokens(std::vector<StyioToken*>&& tokens) {
    if (phase_ != CompilationPhase::Empty) {
      throw std::logic_error("adopt_tokens requires an empty compilation session");
    }
    clear_tokens();
    tokens_ = std::move(tokens);
    transition_to(CompilationPhase::Tokenized, "adopt_tokens");
  }

  std::vector<StyioToken*>& tokens() {
    return tokens_;
  }

  const std::vector<StyioToken*>& tokens() const {
    return tokens_;
  }

  StyioContext* attach_context(StyioContext* ctx) {
    if (phase_ != CompilationPhase::Tokenized) {
      throw std::logic_error("attach_context requires a tokenized compilation session");
    }
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
    if (phase_ != CompilationPhase::Tokenized) {
      throw std::logic_error("attach_ast requires a tokenized compilation session");
    }
    if (ast_ != nullptr && ast_ != ast) {
      delete ast_;
    }
    ast_ = ast;
    transition_to(CompilationPhase::Parsed, "attach_ast");
    return ast_;
  }

  MainBlockAST* ast() {
    return ast_;
  }

  void mark_type_checked() {
    if (phase_ != CompilationPhase::Parsed) {
      throw std::logic_error("mark_type_checked requires a parsed compilation session");
    }
    transition_to(CompilationPhase::Typed, "mark_type_checked");
  }

  StyioIR* attach_ir(StyioIR* ir) {
    if (phase_ != CompilationPhase::Typed) {
      throw std::logic_error("attach_ir requires a type-checked compilation session");
    }
    if (ir_ != nullptr) {
      delete ir_;
    }
    ir_ = ir;
    transition_to(CompilationPhase::Lowered, "attach_ir");
    return ir_;
  }

  StyioIR* ir() {
    return ir_;
  }

  void mark_codegen_ready() {
    if (phase_ != CompilationPhase::Lowered) {
      throw std::logic_error("mark_codegen_ready requires a lowered compilation session");
    }
    transition_to(CompilationPhase::CodegenReady, "mark_codegen_ready");
  }

  void mark_executed() {
    if (phase_ != CompilationPhase::CodegenReady) {
      throw std::logic_error("mark_executed requires a codegen-ready compilation session");
    }
    transition_to(CompilationPhase::Executed, "mark_executed");
  }

  void mark_failed() {
    if (phase_ != CompilationPhase::Empty) {
      transition_to(CompilationPhase::Failed, "mark_failed");
    }
  }

  CompilationPhase phase() const {
    return phase_;
  }

  std::size_t ast_arena_bytes() const {
    return ast_arena_.bytes_used();
  }

  std::size_t token_arena_bytes() const {
    return token_arena_.bytes_used();
  }
};

#endif // STYIO_COMPILATION_SESSION_HPP_
