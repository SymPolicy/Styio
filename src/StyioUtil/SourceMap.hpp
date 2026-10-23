#pragma once
#ifndef STYIO_UTIL_SOURCE_MAP_HPP_
#define STYIO_UTIL_SOURCE_MAP_HPP_

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace styio::util {

/// Unified offset-to-line/column mapping for diagnostics, IDE, and CLI.
/// Replaces three duplicate implementations in SyntaxCheck.cpp, IDE Common.cpp,
/// and main.cpp.
///
/// All line/column values are 0-based internally. 1-based variants are provided
/// for CLI diagnostic output compatibility.
class SourceMap {
public:
  SourceMap() = default;
  explicit SourceMap(std::string_view text);

  /// Rebuild the line table from new source text.
  void rebuild(std::string_view text);

  // ---- 0-based queries (IDE / LSP) ----

  struct Position {
    std::size_t line = 0;
    std::size_t character = 0;
  };

  /// Convert byte offset to 0-based {line, character}.
  Position position_at(std::size_t offset) const;

  /// Convert 0-based {line, character} back to byte offset.
  std::size_t offset_at(Position pos) const;

  // ---- 1-based queries (CLI diagnostics) ----

  /// Convert byte offset to 1-based {line, column}.
  std::pair<std::size_t, std::size_t> position_at_1based(std::size_t offset) const;

  // ---- Line access ----

  std::size_t line_count() const noexcept;
  std::string_view line_text(std::size_t line) const;

  // ---- Interop with legacy line_seps (main.cpp) ----

  /// Build a vector of (start, length) pairs for each line.
  std::vector<std::pair<std::size_t, std::size_t>> build_line_seps() const;

  // ---- Raw access ----

  const std::vector<std::size_t>& line_starts() const noexcept { return line_starts_; }
  std::string_view text() const noexcept { return text_; }

private:
  std::string_view text_;
  std::vector<std::size_t> line_starts_;
};

} // namespace styio::util

#endif // STYIO_UTIL_SOURCE_MAP_HPP_
