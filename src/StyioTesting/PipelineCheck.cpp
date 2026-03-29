#include "StyioTesting/PipelineCheck.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "StyioAST/AST.hpp"
#include "StyioAnalyzer/ASTAnalyzer.hpp"
#include "StyioCodeGen/CodeGenVisitor.hpp"
#include "StyioException/Exception.hpp"
#include "StyioIR/StyioIR.hpp"
#include "StyioJIT/StyioJIT_ORC.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioToString/ToStringVisitor.hpp"
#include "StyioToken/Token.hpp"

#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"

namespace fs = std::filesystem;

namespace styio {
namespace testing {

namespace {

std::string
read_text_file(const fs::path& p) {
  std::ifstream in(p);
  if (!in) {
    throw std::runtime_error("cannot read: " + p.string());
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

/** Strip trailing CR/LF then append a single \\n (stable golden comparison). */
void
normalize_text(std::string& s) {
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
    s.pop_back();
  }
  s.push_back('\n');
}

std::string
capture_subprocess_stdout(const std::string& cmd) {
  std::string out;
  std::array<char, 4096> buf{};
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    return {};
  }
  while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
    out += buf.data();
  }
  pclose(pipe);
  return out;
}

std::string
escape_lexeme(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (unsigned char c : s) {
    if (c == '\n') {
      o += "\\n";
    }
    else if (c == '\r') {
      o += "\\r";
    }
    else if (c == '\t') {
      o += "\\t";
    }
    else if (c == '\\') {
      o += "\\\\";
    }
    else {
      o += static_cast<char>(c);
    }
  }
  return o;
}

std::string
tokens_to_golden(const std::vector<StyioToken*>& tokens) {
  std::ostringstream out;
  for (auto* tok : tokens) {
    out << StyioToken::getTokName(tok->type) << '\t' << escape_lexeme(tok->original) << '\n';
  }
  return out.str();
}

std::pair<std::string, std::vector<std::pair<size_t, size_t>>>
read_styio_source_lines(const fs::path& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("cannot open: " + path.string());
  }
  std::string text;
  std::vector<std::pair<size_t, size_t>> lineseps;
  size_t p = 0;
  std::string line;
  while (std::getline(file, line)) {
    lineseps.push_back(std::make_pair(p, line.size()));
    text += line;
    p += line.size();
    text.push_back('\n');
    p += 1;
  }
  return {text, lineseps};
}

std::string
first_text_diff(const std::string& got, const std::string& exp, const char* artifact) {
  if (got == exp) {
    return {};
  }
  std::istringstream ga(got);
  std::istringstream ea(exp);
  std::string gl, el;
  for (size_t line = 1;; ++line) {
    const bool gok = static_cast<bool>(std::getline(ga, gl));
    const bool eok = static_cast<bool>(std::getline(ea, el));
    if (!gok && !eok) {
      return std::string(artifact) + ": differs (e.g. trailing newline)";
    }
    if (!gok) {
      return std::string(artifact) + ": missing line(s) from " + std::to_string(line);
    }
    if (!eok) {
      return std::string(artifact) + ": extra line(s) from " + std::to_string(line);
    }
    if (gl != el) {
      return std::string(artifact) + ": line " + std::to_string(line) + "\n  exp: " + el
        + "\n  got: " + gl;
    }
  }
}

} // namespace

std::string
run_pipeline_case(const std::string& case_dir, const char* layer5_compiler_exe) {
  const fs::path root(case_dir);
  const fs::path input = root / "input.styio";
  const fs::path gold = root / "expected";

  if (!fs::exists(input)) {
    return "case missing input.styio: " + input.string();
  }

  try {
    auto [code_text, line_seps] = read_styio_source_lines(input);

    /* --- L1: Lexer --- */
    std::vector<StyioToken*> token_list = StyioTokenizer::tokenize(code_text);
    std::string got_tokens = tokens_to_golden(token_list);
    std::string exp_tokens = read_text_file(gold / "tokens.txt");
    normalize_text(got_tokens);
    normalize_text(exp_tokens);
    if (got_tokens != exp_tokens) {
      for (auto* t : token_list) {
        delete t;
      }
      std::string msg =
        std::string("Layer 1 (Lexer): ") + first_text_diff(got_tokens, exp_tokens, "tokens.txt");
      if (std::getenv("STYIO_PIPELINE_DUMP_FULL") != nullptr) {
        msg += "\n--- got tokens (full) ---\n";
        msg += got_tokens;
      }
      return msg;
    }
    for (auto* t : token_list) {
      delete t;
    }

    /* Re-tokenize for parser (parser consumes fresh vector). */
    token_list = StyioTokenizer::tokenize(code_text);
    StyioContext* ctx = StyioContext::Create(
      input.string(), code_text, line_seps, token_list, false);

    MainBlockAST* ast = nullptr;
    try {
      ast = parse_main_block(*ctx);
    } catch (const StyioBaseException& ex) {
      for (auto* t : token_list) {
        delete t;
      }
      delete ctx;
      return std::string("Layer 2 (Parser): ") + ex.what();
    } catch (const std::exception& ex) {
      for (auto* t : token_list) {
        delete t;
      }
      delete ctx;
      return std::string("Layer 2 (Parser): ") + ex.what();
    }

    StyioRepr repr;
    const std::string ast_pre = ast->toString(&repr);

    StyioAnalyzer analyzer;
    analyzer.typeInfer(ast);
    std::string ast_typed = ast->toString(&repr);

    std::string exp_ast = read_text_file(gold / "ast.txt");
    normalize_text(ast_typed);
    normalize_text(exp_ast);
    if (ast_typed != exp_ast) {
      for (auto* t : token_list) {
        delete t;
      }
      delete ctx;
      delete ast;
      return std::string("Layer 2 (Parser/AST typed): ")
        + first_text_diff(ast_typed, exp_ast, "ast.txt")
        + "\n(-- untyped AST for debug --)\n"
        + ast_pre;
    }

    StyioIR* ir = analyzer.toStyioIR(ast);
    std::string got_ir = ir->toString(&repr);
    std::string exp_ir = read_text_file(gold / "styio_ir.txt");
    normalize_text(got_ir);
    normalize_text(exp_ir);
    if (got_ir != exp_ir) {
      for (auto* t : token_list) {
        delete t;
      }
      delete ctx;
      delete ast;
      delete ir;
      return std::string("Layer 3 (StyioIR): ") + first_text_diff(got_ir, exp_ir, "styio_ir.txt");
    }

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    llvm::ExitOnError exit_on_error;
    std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
    StyioToLLVM generator(std::move(jit));
    ir->toLLVMIR(&generator);
    std::string got_llvm = generator.dump_llvm_ir();
    std::string exp_llvm = read_text_file(gold / "llvm_ir.txt");
    normalize_text(got_llvm);
    normalize_text(exp_llvm);
    if (got_llvm != exp_llvm) {
      for (auto* t : token_list) {
        delete t;
      }
      delete ctx;
      delete ast;
      delete ir;
      return std::string("Layer 4 (LLVM IR): ") + first_text_diff(got_llvm, exp_llvm, "llvm_ir.txt");
    }

    if (layer5_compiler_exe != nullptr && std::string(layer5_compiler_exe)[0] != '\0') {
      const std::string cmd = std::string("\"") + layer5_compiler_exe + "\" --file \"" + input.string()
        + "\" 2>/dev/null";
      std::string got_out = capture_subprocess_stdout(cmd);
      std::string exp_out = read_text_file(gold / "stdout.txt");
      normalize_text(got_out);
      normalize_text(exp_out);
      if (got_out != exp_out) {
        for (auto* t : token_list) {
          delete t;
        }
        delete ctx;
        delete ast;
        delete ir;
        return std::string("Layer 5 (run stdout): ")
          + first_text_diff(got_out, exp_out, "stdout.txt");
      }
    }

    for (auto* t : token_list) {
      delete t;
    }
    delete ctx;
    delete ast;
    delete ir;
    return {};
  } catch (const std::exception& ex) {
    return std::string("Pipeline error: ") + ex.what();
  }
}

} // namespace testing
} // namespace styio
