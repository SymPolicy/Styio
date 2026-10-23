#include "SourceMap.hpp"

#include <algorithm>
#include <stdexcept>

namespace styio::util {

SourceMap::SourceMap(std::string_view text) {
  rebuild(text);
}

void SourceMap::rebuild(std::string_view text) {
  text_ = text;
  line_starts_.clear();
  line_starts_.push_back(0);
  for (std::size_t i = 0; i < text_.size(); ++i) {
    if (text_[i] == '\n') {
      line_starts_.push_back(i + 1);
    }
  }
}

SourceMap::Position SourceMap::position_at(std::size_t offset) const {
  if (line_starts_.empty()) {
    return {0, 0};
  }
  offset = std::min(offset, text_.size());
  const auto it = std::upper_bound(line_starts_.begin(), line_starts_.end(), offset);
  const std::size_t line =
    it == line_starts_.begin() ? 0 : static_cast<std::size_t>((it - line_starts_.begin()) - 1);
  return {line, offset - line_starts_[line]};
}

std::size_t SourceMap::offset_at(Position pos) const {
  if (line_starts_.empty() || pos.line >= line_starts_.size()) {
    return text_.size();
  }
  const std::size_t line_start = line_starts_[pos.line];
  const std::size_t line_end =
    pos.line + 1 < line_starts_.size() ? line_starts_[pos.line + 1] : text_.size();
  return std::min(line_start + pos.character, line_end);
}

std::pair<std::size_t, std::size_t> SourceMap::position_at_1based(std::size_t offset) const {
  Position p = position_at(offset);
  return {p.line + 1, p.character + 1};
}

std::size_t SourceMap::line_count() const noexcept {
  return line_starts_.empty() ? 0 : line_starts_.size();
}

std::string_view SourceMap::line_text(std::size_t line) const {
  if (line >= line_starts_.size()) {
    return {};
  }
  const std::size_t start = line_starts_[line];
  std::size_t end =
    line + 1 < line_starts_.size() ? line_starts_[line + 1] : text_.size();
  // Safely trim trailing newline sequences: only \n or \r\n pairs.
  // Standalone \r is NOT a line terminator in this model.
  if (end > start && text_[end - 1] == '\n') {
    --end;
    if (end > start && text_[end - 1] == '\r') {
      --end;
    }
  }
  return text_.substr(start, end - start);
}

std::vector<std::pair<std::size_t, std::size_t>> SourceMap::build_line_seps() const {
  std::vector<std::pair<std::size_t, std::size_t>> seps;
  seps.reserve(line_starts_.size());
  for (std::size_t i = 0; i < line_starts_.size(); ++i) {
    const std::size_t start = line_starts_[i];
    const std::size_t end =
      i + 1 < line_starts_.size() ? line_starts_[i + 1] : text_.size();
    seps.emplace_back(start, end - start);
  }
  return seps;
}

} // namespace styio::util
