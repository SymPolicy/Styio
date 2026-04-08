#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "StyioAST/AST.hpp"
#include "StyioException/Exception.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"

namespace {

std::vector<std::pair<size_t, size_t>>
build_line_seps(const std::string& src) {
  std::vector<std::pair<size_t, size_t>> seps;
  if (src.empty()) {
    seps.push_back({0, 0});
    return seps;
  }

  size_t start = 0;
  for (size_t i = 0; i < src.size(); ++i) {
    if (src[i] == '\n') {
      seps.push_back({start, i - start});
      start = i + 1;
    }
  }
  if (start <= src.size()) {
    seps.push_back({start, src.size() - start});
  }
  return seps;
}

void
free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* t : tokens) {
    delete t;
  }
  tokens.clear();
}

void
fuzz_parse_with_engine_latest(
  const std::string& src,
  const std::vector<std::pair<size_t, size_t>>& line_seps,
  StyioParserEngine engine) {
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = nullptr;
  MainBlockAST* ast = nullptr;

  try {
    tokens = StyioTokenizer::tokenize(src);
    ctx = StyioContext::Create("<fuzz>", src, line_seps, tokens, false);
    ast = parse_main_block_with_engine_latest(*ctx, engine, nullptr);
  } catch (const StyioBaseException&) {
    // expected on malformed inputs
  } catch (...) {
    // keep fuzzing on soft failures; sanitizer handles memory safety issues
  }

  (void)ast;
  StyioAST::destroy_all_tracked_nodes();
  if (ctx != nullptr) {
    delete ctx;
  }
  free_tokens(tokens);
}

} // namespace

extern "C" int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr) {
    return 0;
  }

  std::string src(reinterpret_cast<const char*>(data), size);
  std::vector<std::pair<size_t, size_t>> line_seps = build_line_seps(src);
  fuzz_parse_with_engine_latest(src, line_seps, StyioParserEngine::Legacy);
  fuzz_parse_with_engine_latest(src, line_seps, StyioParserEngine::Nightly);
  return 0;
}
