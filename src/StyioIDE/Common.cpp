#include "Common.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace styio::ide {

namespace {

std::string
percent_decode(const std::string& input) {
  std::string out;
  out.reserve(input.size());

  for (std::size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '%' && i + 2 < input.size()) {
      const char hi = input[i + 1];
      const char lo = input[i + 2];
      auto from_hex = [](char ch) -> int
      {
        if (ch >= '0' && ch <= '9') {
          return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
          return 10 + (ch - 'a');
        }
        if (ch >= 'A' && ch <= 'F') {
          return 10 + (ch - 'A');
        }
        return -1;
      };
      const int hi_value = from_hex(hi);
      const int lo_value = from_hex(lo);
      if (hi_value >= 0 && lo_value >= 0) {
        out.push_back(static_cast<char>((hi_value << 4) | lo_value));
        i += 2;
        continue;
      }
    }
    if (input[i] == '+') {
      out.push_back(' ');
      continue;
    }
    out.push_back(input[i]);
  }

  return out;
}

std::string
percent_encode(const std::string& input) {
  std::ostringstream oss;
  for (unsigned char ch : input) {
    if (std::isalnum(ch) || ch == '/' || ch == '.' || ch == '-' || ch == '_' || ch == '~') {
      oss << static_cast<char>(ch);
      continue;
    }
    oss << '%' << "0123456789ABCDEF"[ch >> 4] << "0123456789ABCDEF"[ch & 0x0F];
  }
  return oss.str();
}

}  // namespace

void
TextBuffer::rebuild_line_starts() {
  line_starts_.clear();
  line_starts_.push_back(0);

  for (std::size_t i = 0; i < text_.size(); ++i) {
    if (text_[i] == '\n') {
      line_starts_.push_back(i + 1);
    }
  }
}

TextBuffer::TextBuffer(std::string text) :
    text_(std::move(text)) {
  rebuild_line_starts();
}

void
TextBuffer::reset(std::string text) {
  text_ = std::move(text);
  rebuild_line_starts();
}

Position
TextBuffer::position_at(std::size_t offset) const {
  if (line_starts_.empty()) {
    return Position{};
  }

  if (offset > text_.size()) {
    offset = text_.size();
  }

  const auto it = std::upper_bound(line_starts_.begin(), line_starts_.end(), offset);
  const std::size_t line = it == line_starts_.begin() ? 0 : static_cast<std::size_t>((it - line_starts_.begin()) - 1);
  return Position{line, offset - line_starts_[line]};
}

std::size_t
TextBuffer::offset_at(Position position) const {
  if (line_starts_.empty()) {
    return 0;
  }

  const std::size_t line = std::min(position.line, line_starts_.size() - 1);
  const std::size_t line_start = line_starts_[line];
  const std::size_t next_line_start = line + 1 < line_starts_.size() ? line_starts_[line + 1] : text_.size();
  const std::size_t line_len = next_line_start >= line_start ? next_line_start - line_start : 0;
  return line_start + std::min(position.character, line_len);
}

std::vector<std::pair<std::size_t, std::size_t>>
TextBuffer::build_line_seps() const {
  std::vector<std::pair<std::size_t, std::size_t>> seps;
  std::size_t line_start = 0;
  std::size_t line_len = 0;
  for (char ch : text_) {
    if (ch == '\n') {
      seps.emplace_back(line_start, line_len);
      line_start += line_len + 1;
      line_len = 0;
      continue;
    }
    line_len += 1;
  }
  if (!text_.empty() && text_.back() != '\n') {
    seps.emplace_back(line_start, line_len);
  }
  return seps;
}

std::string
path_from_uri(const std::string& uri) {
  constexpr const char* k_file_prefix = "file://";
  if (uri.rfind(k_file_prefix, 0) != 0) {
    return uri;
  }

  std::string decoded = percent_decode(uri.substr(7));
  if (!decoded.empty() && decoded[0] != '/') {
    decoded.insert(decoded.begin(), '/');
  }
  return decoded;
}

std::string
uri_from_path(const std::string& path) {
  std::filesystem::path fs_path(path);
  std::string normalized = fs_path.lexically_normal().string();
  if (normalized.empty() || normalized[0] != '/') {
    normalized = "/" + normalized;
  }
  return "file://" + percent_encode(normalized);
}

std::string
to_string(PositionKind kind) {
  switch (kind) {
    case PositionKind::TopLevel:
      return "TopLevel";
    case PositionKind::StmtStart:
      return "StmtStart";
    case PositionKind::Expr:
      return "Expr";
    case PositionKind::Type:
      return "Type";
    case PositionKind::Pattern:
      return "Pattern";
    case PositionKind::MemberAccess:
      return "MemberAccess";
    case PositionKind::ImportPath:
      return "ImportPath";
    case PositionKind::CallArg:
      return "CallArg";
    case PositionKind::AttrName:
      return "AttrName";
  }
  return "Expr";
}

std::string
to_string(SymbolKind kind) {
  switch (kind) {
    case SymbolKind::Variable:
      return "variable";
    case SymbolKind::Function:
      return "function";
    case SymbolKind::Parameter:
      return "parameter";
    case SymbolKind::Builtin:
      return "builtin";
  }
  return "variable";
}

std::string
to_string(CompletionItemKind kind) {
  switch (kind) {
    case CompletionItemKind::Variable:
      return "variable";
    case CompletionItemKind::Function:
      return "function";
    case CompletionItemKind::Type:
      return "type";
    case CompletionItemKind::Keyword:
      return "keyword";
    case CompletionItemKind::Snippet:
      return "snippet";
    case CompletionItemKind::Property:
      return "property";
    case CompletionItemKind::Module:
      return "module";
  }
  return "variable";
}

std::string
to_string(CompletionSource source) {
  switch (source) {
    case CompletionSource::Local:
      return "local";
    case CompletionSource::Imported:
      return "imported";
    case CompletionSource::Builtin:
      return "builtin";
    case CompletionSource::Keyword:
      return "keyword";
    case CompletionSource::Snippet:
      return "snippet";
  }
  return "local";
}

}  // namespace styio::ide
