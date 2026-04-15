#pragma once

#ifndef STYIO_IDE_COMMON_HPP_
#define STYIO_IDE_COMMON_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace styio::ide {

using FileId = std::uint32_t;
using DocumentVersion = std::int64_t;
using SnapshotId = std::uint64_t;
using ProjectId = std::uint32_t;
using ModuleId = std::uint32_t;
using SymbolId = std::uint32_t;
using ScopeId = std::uint32_t;
using TypeId = std::uint32_t;

struct Position
{
  std::size_t line = 0;
  std::size_t character = 0;
};

struct TextRange
{
  std::size_t start = 0;
  std::size_t end = 0;

  bool contains(std::size_t offset) const {
    return start <= offset && offset <= end;
  }

  std::size_t length() const {
    return end >= start ? end - start : 0;
  }
};

enum class PositionKind
{
  TopLevel,
  StmtStart,
  Expr,
  Type,
  Pattern,
  MemberAccess,
  ImportPath,
  CallArg,
  AttrName,
};

enum class SymbolKind
{
  Variable,
  Function,
  Parameter,
  Builtin,
};

enum class CompletionItemKind
{
  Variable,
  Function,
  Type,
  Keyword,
  Snippet,
  Property,
  Module,
};

enum class CompletionSource
{
  Local,
  Imported,
  Builtin,
  Keyword,
  Snippet,
};

enum class DiagnosticSeverity
{
  Error = 1,
  Warning = 2,
  Information = 3,
  Hint = 4,
};

struct Diagnostic
{
  TextRange range;
  DiagnosticSeverity severity = DiagnosticSeverity::Error;
  std::string source;
  std::string message;
};

struct Location
{
  std::string path;
  TextRange range;
};

struct CompletionContext
{
  FileId file_id = 0;
  SnapshotId snapshot_id = 0;
  std::size_t offset = 0;
  std::string prefix;
  PositionKind position_kind = PositionKind::Expr;
  std::vector<std::string> expected_tokens;
  std::vector<std::string> expected_categories;
  ScopeId scope_id = 0;
  TypeId receiver_type_id = 0;
};

struct CompletionItem
{
  std::string label;
  CompletionItemKind kind = CompletionItemKind::Variable;
  std::string insert_text;
  std::string detail;
  int sort_score = 0;
  CompletionSource source = CompletionSource::Local;
};

struct DocumentSymbol
{
  std::string name;
  SymbolKind kind = SymbolKind::Variable;
  TextRange range;
  TextRange selection_range;
  std::string detail;
};

struct HoverResult
{
  std::string contents;
  std::optional<TextRange> range;
};

struct TextBuffer
{
private:
  std::string text_;
  std::vector<std::size_t> line_starts_;

  void rebuild_line_starts();

public:
  TextBuffer() = default;
  explicit TextBuffer(std::string text);

  void reset(std::string text);

  const std::string& text() const {
    return text_;
  }

  bool empty() const {
    return text_.empty();
  }

  std::size_t size() const {
    return text_.size();
  }

  Position position_at(std::size_t offset) const;
  std::size_t offset_at(Position position) const;
  std::vector<std::pair<std::size_t, std::size_t>> build_line_seps() const;
};

std::string path_from_uri(const std::string& uri);
std::string uri_from_path(const std::string& path);

std::string to_string(PositionKind kind);
std::string to_string(SymbolKind kind);
std::string to_string(CompletionItemKind kind);
std::string to_string(CompletionSource source);

}  // namespace styio::ide

#endif
