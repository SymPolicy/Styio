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
    if (std::isalnum(ch) || ch == '/' || ch == ':' || ch == '.' || ch == '-' || ch == '_' || ch == '~') {
      oss << static_cast<char>(ch);
      continue;
    }
    oss << '%' << "0123456789ABCDEF"[ch >> 4] << "0123456789ABCDEF"[ch & 0x0F];
  }
  return oss.str();
}

std::size_t
utf8_sequence_bytes(unsigned char lead) {
  if ((lead & 0x80U) == 0U) {
    return 1;
  }
  if ((lead & 0xE0U) == 0xC0U) {
    return 2;
  }
  if ((lead & 0xF0U) == 0xE0U) {
    return 3;
  }
  if ((lead & 0xF8U) == 0xF0U) {
    return 4;
  }
  return 1;
}

std::size_t
utf16_units_for_utf8_lead(unsigned char lead) {
  return (lead & 0xF8U) == 0xF0U ? 2 : 1;
}

std::size_t
utf16_units_between(const std::string& text, std::size_t start, std::size_t end) {
  start = std::min(start, text.size());
  end = std::min(end, text.size());
  if (end < start) {
    return 0;
  }

  std::size_t units = 0;
  for (std::size_t cursor = start; cursor < end;) {
    const unsigned char lead = static_cast<unsigned char>(text[cursor]);
    const std::size_t width = std::min(utf8_sequence_bytes(lead), end - cursor);
    units += utf16_units_for_utf8_lead(lead);
    cursor += std::max<std::size_t>(1, width);
  }
  return units;
}

}  // namespace

std::shared_ptr<const TextBuffer::Storage>
TextBuffer::make_storage(std::string text) {
  auto storage = std::make_shared<Storage>();
  storage->text = std::move(text);
  storage->line_starts.push_back(0);
  for (std::size_t i = 0; i < storage->text.size(); ++i) {
    if (storage->text[i] == '\n') {
      storage->line_starts.push_back(i + 1);
    }
  }
  return storage;
}

const std::vector<std::size_t>&
TextBuffer::line_starts() const {
  static const std::vector<std::size_t> empty;
  return storage_ ? storage_->line_starts : empty;
}

TextBuffer::TextBuffer(std::string text) {
  reset(std::move(text));
}

void
TextBuffer::reset(std::string text) {
  storage_ = make_storage(std::move(text));
}

Position
TextBuffer::position_at(std::size_t offset) const {
  const auto& starts = line_starts();
  const auto& source = text();
  if (starts.empty()) {
    return Position{};
  }

  if (offset > source.size()) {
    offset = source.size();
  }

  const auto it = std::upper_bound(starts.begin(), starts.end(), offset);
  const std::size_t line = it == starts.begin()
    ? 0
    : static_cast<std::size_t>((it - starts.begin()) - 1);
  return Position{line, offset - starts[line]};
}

Position
TextBuffer::utf16_position_at(std::size_t offset) const {
  const Position byte_position = position_at(offset);
  const auto& starts = line_starts();
  const auto& source = text();
  if (starts.empty()) {
    return byte_position;
  }

  const std::size_t line = std::min(byte_position.line, starts.size() - 1);
  return Position{
    line,
    utf16_units_between(source, starts[line], std::min(offset, source.size()))};
}

std::size_t
TextBuffer::offset_at(Position position) const {
  const auto& starts = line_starts();
  const auto& source = text();
  if (starts.empty()) {
    return 0;
  }

  const std::size_t line = std::min(position.line, starts.size() - 1);
  const std::size_t line_start = starts[line];
  const std::size_t next_line_start =
    line + 1 < starts.size() ? starts[line + 1] : source.size();
  const std::size_t line_len = next_line_start >= line_start ? next_line_start - line_start : 0;
  return line_start + std::min(position.character, line_len);
}

std::size_t
TextBuffer::utf16_length(TextRange range) const {
  return utf16_units_between(text(), range.start, range.end);
}

std::vector<std::pair<std::size_t, std::size_t>>
TextBuffer::build_line_seps() const {
  std::vector<std::pair<std::size_t, std::size_t>> seps;
  std::size_t line_start = 0;
  std::size_t line_len = 0;
  const auto& source = text();
  for (char ch : source) {
    if (ch == '\n') {
      seps.emplace_back(line_start, line_len);
      line_start += line_len + 1;
      line_len = 0;
      continue;
    }
    line_len += 1;
  }
  if (!source.empty() && source.back() != '\n') {
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
#if defined(_WIN32)
  std::replace(decoded.begin(), decoded.end(), '/', '\\');
  if (decoded.size() >= 3
      && decoded[0] == '\\'
      && std::isalpha(static_cast<unsigned char>(decoded[1]))
      && decoded[2] == ':') {
    decoded.erase(decoded.begin());
  } else if (!decoded.empty() && decoded[0] != '\\') {
    decoded.insert(decoded.begin(), '\\');
  }
#else
  if (!decoded.empty() && decoded[0] != '/') {
    decoded.insert(decoded.begin(), '/');
  }
#endif
  return decoded;
}

std::string
uri_from_path(const std::string& path) {
  std::filesystem::path fs_path(path);
  std::string normalized = fs_path.lexically_normal().generic_string();
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
