/*
  styio_core_bench — in-repo C++ benchmark binary.
  Uses std::chrono::steady_clock, outputs JSON to stdout or file.
  No third-party dependencies beyond the compiler itself.
*/

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "bench_utils.hpp"

// [Styio]
#include "StyioAST/AST.hpp"
#include "StyioException/Exception.hpp"
#include "StyioLowering/AstToStyioIRLowerer.hpp"
#include "StyioLowering/AstToStyioIRStage.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioResourceTopology/ResourceTopology.hpp"
#include "StyioSema/SemanticAnalysis.hpp"
#include "StyioSession/CompilationSession.hpp"

namespace {

#if defined(_WIN32)
#define styio_bench_popen _popen
#define styio_bench_pclose _pclose
constexpr const char* kGitShaCommand = "git rev-parse --short HEAD 2>nul";
#else
#define styio_bench_popen popen
#define styio_bench_pclose pclose
constexpr const char* kGitShaCommand = "git rev-parse --short HEAD 2>/dev/null";
#endif

// -------------------------------------------------------------------
// Synthetic source generators
// -------------------------------------------------------------------

std::string gen_many_bindings(int count) {
  std::ostringstream os;
  for (int i = 0; i < count; ++i)
    os << "x_" << i << " := " << (i * 7 % 1000) << ";\n";
  return os.str();
}

// Flat binop chain: x := 1 + 2 + 3 + ... + N;
std::string gen_deep_expr(int depth) {
  std::ostringstream os;
  os << "x := 1";
  for (int i = 2; i <= depth; ++i)
    os << " + " << i;
  os << ";\n";
  return os.str();
}

std::string gen_large_file(int lines) {
  std::ostringstream os;
  for (int i = 0; i < lines; ++i) {
    if (i % 5 == 0)
      os << "// comment " << i << "\n";
    else if (i % 7 == 0)
      os << "\n";
    else
      os << "v_" << i << " := " << (i % 100) << ";\n";
  }
  return os.str();
}

std::string gen_many_names(int count) {
  std::ostringstream os;
  for (int i = 0; i < count; ++i)
    os << "n_" << i << " := " << i << ";\n";
  os << "sum := ";
  for (int i = 0; i < count; ++i) {
    if (i > 0) os << " + ";
    os << "n_" << i;
  }
  os << ";\n";
  return os.str();
}

std::string gen_typed_bindings(int count) {
  std::ostringstream os;
  for (int i = 0; i < count; ++i)
    os << "t_" << i << " : i64 := " << (i * 3) << ";\n";
  for (int i = 0; i < count / 5; ++i)
    os << "c_" << i << " : i64 := t_" << i << " + t_" << (i + count/2) << ";\n";
  return os.str();
}

std::string gen_resource_ops(int count) {
  std::ostringstream os;
  for (int i = 0; i < count; ++i) {
    os << "@resource r_" << i << " : file {\n"
       << "  path := \"/tmp/f_" << i << "\";\n"
       << "  @method write { push(\"data_" << i << "\"); }\n"
       << "}\n";
  }
  return os.str();
}

std::string gen_error_source(int count) {
  std::ostringstream os;
  for (int i = 0; i < count; ++i) {
    if (i % 3 == 0)
      os << "bad_" << i << " := undefined_func_" << i << "();\n";
    else if (i % 3 == 1)
      os << "bad_" << i << " : i64 := \"not_an_int\";\n";
    else
      os << "bad_" << i << " := 1 + ;\n";
  }
  return os.str();
}

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

int bench_iters(const char* env_name, int default_val) {
  const char* v = std::getenv(env_name);
  if (!v) return default_val;
  int p = std::atoi(v);
  return p > 0 ? p : default_val;
}

std::string get_git_sha() {
  FILE* fp = styio_bench_popen(kGitShaCommand, "r");
  if (!fp) return "unknown";
  char buf[64] = {};
  if (!fgets(buf, sizeof(buf), fp)) { styio_bench_pclose(fp); return "unknown"; }
  styio_bench_pclose(fp);
  std::string s(buf);
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
  return s;
}

std::string get_build_type() {
#ifdef NDEBUG
  return "Release";
#else
  return "Debug";
#endif
}

std::vector<std::pair<std::size_t, std::size_t>>
build_line_seps(const std::string& text) {
  std::vector<std::pair<std::size_t, std::size_t>> seps;
  std::size_t start = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\n') {
      seps.emplace_back(start, i - start);
      start = i + 1;
    }
  }
  if (start <= text.size())
    seps.emplace_back(start, text.size() - start);
  return seps;
}

StyioContext* make_context(
    const std::string& fname,
    const std::string& code,
    std::vector<StyioToken*>& tokens) {
  return StyioContext::Create(fname, code, build_line_seps(code), tokens, false);
}

/// Safely parse — catches and discards exceptions for benchmarking.
MainBlockAST* safe_parse(StyioContext& ctx) {
  try {
    return parse_main_block_with_engine_latest(
        ctx, StyioParserEngine::Nightly, nullptr);
  } catch (const StyioBaseException&) {
    return nullptr;
  }
}

} // anonymous namespace

namespace {

/// Helper: capture route cache counters from a StyioContext into a BenchmarkSample.
styio::bench::BenchmarkSample capture_route_cache_sample(
    const StyioContext& ctx, const std::string& label) {
  styio::bench::BenchmarkSample s;
  s.phase = "route_cache";
  s.label = label;
  s.route_cache_scan_count = static_cast<int64_t>(ctx.route_scan_count());
  s.route_cache_hit_count = static_cast<int64_t>(ctx.route_cache_hit_count());
  s.route_cache_miss_count = static_cast<int64_t>(ctx.route_cache_miss_count());
  s.route_cache_disabled_count = static_cast<int64_t>(ctx.route_cache_disabled_count());
  return s;
}

} // namespace

int main(int argc, char** argv) {
  using namespace styio::bench;

  BenchmarkResult result;
  result.git_sha = get_git_sha();
  result.build_type = get_build_type();
  result.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  const int warmup  = bench_iters("STYIO_BENCH_WARMUP", 2);
  const int measured = bench_iters("STYIO_BENCH_MEASURED", 5);

  // --- 1. lex_large_file ---
  {
    std::string code = gen_large_file(10000);
    result.samples.push_back(run_benchmark("lex", "large_file_10k", warmup, measured, [&]() {
      auto metrics = StyioTokenizerMetrics{};
      auto tokens = StyioTokenizer::tokenizeWithMetrics(code, &metrics);
      for (auto* t : tokens) delete t;
    }));
  }

  // --- 2. parse_many_stmts ---
  {
    std::string code = gen_many_bindings(1000);
    result.samples.push_back(run_benchmark("parse", "many_stmts_1k", warmup, measured, [&]() {
      CompilationSession session;
      auto tokens = StyioTokenizer::tokenize(code);
      session.adopt_tokens(std::move(tokens));
      auto* ctx = make_context("bench.styio", code, session.tokens());
      session.attach_context(ctx);
      auto* ast = safe_parse(*ctx);
      if (ast) session.attach_ast(ast);
    }));
  }

  // --- 3. parse_deep_expr ---
  {
    std::string code = gen_deep_expr(200);
    result.samples.push_back(run_benchmark("parse", "deep_expr_200", warmup, measured, [&]() {
      CompilationSession session;
      auto tokens = StyioTokenizer::tokenize(code);
      session.adopt_tokens(std::move(tokens));
      auto* ctx = make_context("bench.styio", code, session.tokens());
      session.attach_context(ctx);
      auto* ast = safe_parse(*ctx);
      if (ast) session.attach_ast(ast);
    }));
  }

  // --- 4. name_resolution ---
  {
    std::string code = gen_many_names(5000);
    result.samples.push_back(run_benchmark("sema", "name_resolution_5k", warmup, measured, [&]() {
      CompilationSession session;
      auto tokens = StyioTokenizer::tokenize(code);
      session.adopt_tokens(std::move(tokens));
      auto* ctx = make_context("bench.styio", code, session.tokens());
      session.attach_context(ctx);
      auto* ast = safe_parse(*ctx);
      if (!ast) return;
      session.attach_ast(ast);
      AstToStyioIRLowerer lowerer;
      try { styio::sema::require_semantic_analysis(ast, &lowerer); }
      catch (const StyioBaseException&) {}
      session.mark_type_checked();
    }));
  }

  // --- 5. type_check_many_bindings ---
  {
    std::string code = gen_typed_bindings(1000);
    result.samples.push_back(run_benchmark("type", "typed_bindings_1k", warmup, measured, [&]() {
      CompilationSession session;
      auto tokens = StyioTokenizer::tokenize(code);
      session.adopt_tokens(std::move(tokens));
      auto* ctx = make_context("bench.styio", code, session.tokens());
      session.attach_context(ctx);
      auto* ast = safe_parse(*ctx);
      if (!ast) return;
      session.attach_ast(ast);
      AstToStyioIRLowerer lowerer;
      try { styio::sema::require_semantic_analysis(ast, &lowerer); }
      catch (const StyioBaseException&) {}
      session.mark_type_checked();
    }));
  }

  // --- 6. topology_large_dag ---
  {
    std::string code = gen_resource_ops(200);
    result.samples.push_back(run_benchmark("topology", "resource_dag_200", warmup, measured, [&]() {
      CompilationSession session;
      auto tokens = StyioTokenizer::tokenize(code);
      session.adopt_tokens(std::move(tokens));
      auto* ctx = make_context("bench.styio", code, session.tokens());
      session.attach_context(ctx);
      auto* ast = safe_parse(*ctx);
      if (!ast) return;
      session.attach_ast(ast);
      try { styio::resource_topology::validate_or_throw(ast, "benchmark"); }
      catch (const std::exception&) {}
    }));
  }

  // --- 7. diag_many_errors ---
  {
    std::string code = gen_error_source(100);
    result.samples.push_back(run_benchmark("diag", "many_errors_100", warmup, measured, [&]() {
      CompilationSession session;
      auto tokens = StyioTokenizer::tokenize(code);
      session.adopt_tokens(std::move(tokens));
      auto* ctx = make_context("bench.styio", code, session.tokens());
      session.attach_context(ctx);
      try {
        auto* ast = safe_parse(*ctx);
        if (ast) session.attach_ast(ast);
      } catch (const StyioBaseException& ex) {
        volatile auto ignored = ex.what(); (void)ignored;
      }
    }));
  }

  // --- 8. runtime baseline ---
  {
    result.samples.push_back(run_benchmark("runtime", "task_spawn_1k_nop", warmup, measured, [&]() {
      volatile int sum = 0;
      for (int i = 0; i < 1000; ++i) sum += i;
      (void)sum;
    }));
  }

  // --- 9. Route cache stats (single-run capture, not timed) ---
  {
    auto capture_route_cache_for = [&](const std::string& label, const std::string& code) {
      CompilationSession session;
      auto tokens = StyioTokenizer::tokenize(code);
      session.adopt_tokens(std::move(tokens));
      auto* ctx = make_context("bench.styio", code, session.tokens());
      session.attach_context(ctx);
      auto* ast = safe_parse(*ctx);
      if (ast) session.attach_ast(ast);
      BenchmarkSample s = capture_route_cache_sample(*ctx, label);
      result.samples.push_back(std::move(s));
    };

    capture_route_cache_for("many_stmts_1k", gen_many_bindings(1000));
    capture_route_cache_for("deep_expr_200", gen_deep_expr(200));
    capture_route_cache_for("name_resolution_5k", gen_many_names(5000));
    capture_route_cache_for("typed_bindings_1k", gen_typed_bindings(1000));
    capture_route_cache_for("resource_dag_200", gen_resource_ops(200));
  }

  // --- Output ---
  std::string json = result.to_json();

  const char* output_path = nullptr;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);
    if ((arg == "--output" || arg == "-o") && i + 1 < argc)
      output_path = argv[++i];
    else if (arg == "--stdout")
      output_path = nullptr;
  }

  if (output_path) {
    std::ofstream out(output_path);
    if (!out) { std::cerr << "ERROR: cannot write\n"; return 1; }
    out << json;
  } else {
    std::cout << json << "\n";
  }
  return 0;
}
