#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "StyioIDE/Common.hpp"
#include "StyioIDE/VFS.hpp"
#include "StyioLSP/Server.hpp"
#include "llvm/Support/JSON.h"
#include "fuzz_ide_common.hpp"

namespace {

llvm::json::Object
lsp_position(std::size_t line, std::size_t character) {
  return llvm::json::Object{
    {"line", static_cast<std::int64_t>(line)},
    {"character", static_cast<std::int64_t>(character)}};
}

llvm::json::Object
lsp_range(const styio::ide::TextBuffer& buffer, std::size_t start, std::size_t end) {
  const styio::ide::Position start_pos = buffer.position_at(start);
  const styio::ide::Position end_pos = buffer.position_at(end);
  return llvm::json::Object{
    {"start", lsp_position(start_pos.line, start_pos.character)},
    {"end", lsp_position(end_pos.line, end_pos.character)}};
}

}  // namespace

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr) {
    return 0;
  }

  try {
    const std::string source = styio::fuzz::make_printable_source(data, size);
    const std::string root = styio::fuzz::temp_workspace_root("styio-ide-fuzz-lsp");
    const std::string path = (std::filesystem::path(root) / "lsp.styio").string();
    const std::string uri = styio::ide::uri_from_path(path);

    styio::lsp::Server server;
    (void)server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", llvm::json::Object{{"rootUri", styio::ide::uri_from_path(root)}}}});

    (void)server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "textDocument/didOpen"},
      {"params", llvm::json::Object{
         {"textDocument", llvm::json::Object{
            {"uri", uri},
            {"version", 1},
            {"text", source}}}}}});

    const std::string mutated = source + (size > 0 ? std::string(1, static_cast<char>('a' + (data[0] % 26))) : std::string{});
    const styio::ide::TextBuffer buffer(source);
    const std::size_t start = source.empty() ? 0 : static_cast<std::size_t>(data[0]) % source.size();
    const std::size_t width = size > 1 ? static_cast<std::size_t>(data[1] % 4) : 0;
    const std::size_t end = std::min(start + width, source.size());

    llvm::json::Array changes;
    if (size > 2 && (data[2] % 2) == 0) {
      changes.push_back(llvm::json::Object{{"text", mutated}});
    } else {
      changes.push_back(llvm::json::Object{
        {"range", lsp_range(buffer, start, end)},
        {"text", mutated.substr(mutated.size() > 4 ? mutated.size() - 4 : 0)}});
    }

    (void)server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "textDocument/didChange"},
      {"params", llvm::json::Object{
         {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 2}}},
         {"contentChanges", std::move(changes)}}}});

    const styio::ide::TextBuffer mutated_buffer(mutated);
    const std::size_t query_offset = mutated.empty() ? 0 : std::min<std::size_t>(mutated.size() - 1, size > 3 ? data[3] % mutated.size() : 0);
    const styio::ide::Position pos = mutated_buffer.position_at(query_offset);

    (void)server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 2},
      {"method", "textDocument/completion"},
      {"params", llvm::json::Object{
         {"textDocument", llvm::json::Object{{"uri", uri}}},
         {"position", lsp_position(pos.line, pos.character)}}}});

    (void)server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 3},
      {"method", "textDocument/documentSymbol"},
      {"params", llvm::json::Object{{"textDocument", llvm::json::Object{{"uri", uri}}}}}});

    (void)server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 4},
      {"method", "textDocument/semanticTokens/full"},
      {"params", llvm::json::Object{{"textDocument", llvm::json::Object{{"uri", uri}}}}}});

    (void)server.drain_runtime();
  } catch (...) {
    // Keep fuzzing on soft LSP/parse failures; sanitizers handle hard faults.
  }

  return 0;
}
