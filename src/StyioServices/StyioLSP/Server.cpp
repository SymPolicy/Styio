#include "Server.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <unordered_set>
#include <sstream>
#include <string_view>
#include <utility>

#include "StyioServices/DiagnosticContract.hpp"

namespace styio::lsp {

namespace {

constexpr std::size_t kRuntimeDrainBudgetPerLoop = 1;
constexpr std::size_t kBackgroundWorkBudgetPerLoop = 1;
constexpr std::size_t kMaxContentLength = 16 * 1024 * 1024;

enum class MessageReadStatus
{
  Message,
  Skip,
  End,
};

std::size_t
utf8_sequence_length(unsigned char lead) {
  if ((lead & 0x80u) == 0) {
    return 1;
  }
  if ((lead & 0xE0u) == 0xC0u) {
    return 2;
  }
  if ((lead & 0xF0u) == 0xE0u) {
    return 3;
  }
  if ((lead & 0xF8u) == 0xF0u) {
    return 4;
  }
  return 1;
}

std::size_t
utf16_units_for_utf8_lead(unsigned char lead) {
  return utf8_sequence_length(lead) == 4 ? 2 : 1;
}

std::size_t
utf16_units_between(const std::string& text, std::size_t start, std::size_t end) {
  std::size_t units = 0;
  std::size_t cursor = start;
  while (cursor < end && cursor < text.size()) {
    const std::size_t length = std::min(utf8_sequence_length(static_cast<unsigned char>(text[cursor])), text.size() - cursor);
    units += utf16_units_for_utf8_lead(static_cast<unsigned char>(text[cursor]));
    cursor += length;
  }
  return units;
}

std::optional<std::pair<std::size_t, std::size_t>>
line_bounds(const std::string& text, std::size_t line) {
  std::size_t start = 0;
  std::size_t current_line = 0;
  while (current_line < line) {
    const std::size_t newline = text.find('\n', start);
    if (newline == std::string::npos) {
      return std::nullopt;
    }
    start = newline + 1;
    current_line += 1;
  }

  std::size_t end = text.find('\n', start);
  if (end == std::string::npos) {
    end = text.size();
  }
  if (end > start && text[end - 1] == '\r') {
    end -= 1;
  }
  return std::make_pair(start, end);
}

std::optional<std::size_t>
offset_at_lsp_position(const styio::ide::TextBuffer& buffer, const llvm::json::Object& position) {
  const std::size_t line = static_cast<std::size_t>(position.getInteger("line").value_or(-1));
  const std::size_t target_units = static_cast<std::size_t>(position.getInteger("character").value_or(-1));
  const std::string& text = buffer.text();
  const auto bounds = line_bounds(text, line);
  if (!bounds.has_value()) {
    return std::nullopt;
  }

  std::size_t units = 0;
  std::size_t cursor = bounds->first;
  while (cursor < bounds->second) {
    if (units == target_units) {
      return cursor;
    }
    const unsigned char lead = static_cast<unsigned char>(text[cursor]);
    const std::size_t length = std::min(utf8_sequence_length(lead), text.size() - cursor);
    const std::size_t char_units = utf16_units_for_utf8_lead(lead);
    if (units + char_units > target_units) {
      return std::nullopt;
    }
    units += char_units;
    cursor += length;
  }

  if (units == target_units) {
    return bounds->second;
  }
  return std::nullopt;
}

std::optional<styio::ide::TextRange>
range_from_lsp_range(const styio::ide::TextBuffer& buffer, const llvm::json::Object& range) {
  const auto* start = range.getObject("start");
  const auto* end = range.getObject("end");
  if (start == nullptr || end == nullptr) {
    return std::nullopt;
  }
  const auto start_offset = offset_at_lsp_position(buffer, *start);
  const auto end_offset = offset_at_lsp_position(buffer, *end);
  if (!start_offset.has_value() || !end_offset.has_value() || *start_offset > *end_offset) {
    return std::nullopt;
  }
  return styio::ide::TextRange{*start_offset, *end_offset};
}

llvm::json::Object
position_to_lsp_object(const styio::ide::Position& position) {
  return llvm::json::Object{
    {"line", static_cast<std::int64_t>(position.line)},
    {"character", static_cast<std::int64_t>(position.character)}};
}

styio::ide::Position
position_at_lsp_offset(const styio::ide::TextBuffer& buffer, std::size_t offset) {
  const styio::ide::Position byte_position = buffer.position_at(offset);
  const std::size_t line_start = buffer.offset_at(styio::ide::Position{byte_position.line, 0});
  return styio::ide::Position{
    byte_position.line,
    utf16_units_between(buffer.text(), line_start, offset)};
}

llvm::json::Object
to_lsp_range(const styio::ide::TextBuffer& buffer, styio::ide::TextRange range) {
  const styio::ide::Position start = position_at_lsp_offset(buffer, range.start);
  const styio::ide::Position end = position_at_lsp_offset(buffer, range.end);
  return llvm::json::Object{
    {"start", llvm::json::Object{{"line", static_cast<std::int64_t>(start.line)}, {"character", static_cast<std::int64_t>(start.character)}}},
    {"end", llvm::json::Object{{"line", static_cast<std::int64_t>(end.line)}, {"character", static_cast<std::int64_t>(end.character)}}}};
}

llvm::json::Object
inlay_hint_at_offset(const styio::ide::TextBuffer& buffer, std::size_t offset, const std::string& label) {
  return llvm::json::Object{
    {"position", position_to_lsp_object(position_at_lsp_offset(buffer, offset))},
    {"label", label},
    {"kind", static_cast<std::int64_t>(2)},
    {"paddingRight", true}};
}

std::vector<llvm::json::Object>
parameter_inlay_hints_for_range(
  styio::ide::IdeService& service,
  const std::string& uri,
  const styio::ide::TextRange& request_range
) {
  std::vector<llvm::json::Object> hints;
  const auto snapshot = service.snapshot_for_uri(uri);
  styio::ide::SyntaxParser syntax_parser;
  const styio::ide::SyntaxSnapshot syntax = syntax_parser.parse(*snapshot);

  auto add_hint_for_offset = [&](std::size_t offset)
  {
    if (offset < request_range.start || offset >= request_range.end) {
      return;
    }
    const auto context = service.completion_context(uri, snapshot->buffer.position_at(offset));
    if (context.expected_param_name.empty()) {
      return;
    }
    hints.push_back(inlay_hint_at_offset(snapshot->buffer, offset, context.expected_param_name + ":"));
  };

  for (std::size_t i = 0; i < syntax.tokens.size(); ++i) {
    const auto& token = syntax.tokens[i];
    if (token.type != StyioTokenType::TOK_LPAREN) {
      continue;
    }

    const auto matching = syntax.matching_tokens.find(i);
    if (matching == syntax.matching_tokens.end() || matching->second <= i) {
      continue;
    }

    const auto callee_index = syntax.previous_non_trivia_index(token.range.start);
    if (!callee_index.has_value() || syntax.tokens[*callee_index].type != StyioTokenType::NAME) {
      continue;
    }

    const std::size_t call_end_index = matching->second;
    std::size_t segment_start = i + 1;
    std::size_t depth = 0;

    auto flush_segment = [&](std::size_t segment_end)
    {
      if (segment_start >= segment_end) {
        return;
      }
      for (std::size_t j = segment_start; j < segment_end; ++j) {
        const auto& segment_token = syntax.tokens[j];
        if (segment_token.is_trivia() || segment_token.type == StyioTokenType::TOK_EOF) {
          continue;
        }
        add_hint_for_offset(segment_token.range.start);
        break;
      }
    };

    for (std::size_t j = i + 1; j < call_end_index; ++j) {
      const StyioTokenType type = syntax.tokens[j].type;
      if (type == StyioTokenType::TOK_LPAREN
          || type == StyioTokenType::TOK_LBOXBRAC
          || type == StyioTokenType::TOK_LCURBRAC) {
        depth += 1;
        continue;
      }
      if ((type == StyioTokenType::TOK_RPAREN
           || type == StyioTokenType::TOK_RBOXBRAC
           || type == StyioTokenType::TOK_RCURBRAC)
          && depth > 0) {
        depth -= 1;
        continue;
      }
      if (type == StyioTokenType::TOK_COMMA && depth == 0) {
        flush_segment(j);
        segment_start = j + 1;
      }
    }

    flush_segment(call_end_index);
  }

  return hints;
}

struct RenameEdit
{
  std::string uri;
  styio::ide::TextRange range;
  llvm::json::Object edit;
};

llvm::json::Object
to_lsp_diagnostic_object(
  const styio::ide::TextBuffer& buffer,
  const styio::ide::Diagnostic& diagnostic
) {
  llvm::json::Object item{
    {"range", to_lsp_range(buffer, diagnostic.range)},
    {"severity", static_cast<std::int64_t>(diagnostic.severity)},
    {"source", diagnostic.source},
    {"message", diagnostic.message}};
  if (!diagnostic.code.empty()) {
    item["code"] = diagnostic.code;
  }
  std::string phase = diagnostic.phase;
  if (phase.empty() && diagnostic.code.rfind("STYIO_", 0) == 0) {
    phase = styio::services::diagnostics::diagnostic_phase_for_code(diagnostic.code);
  }
  if (!phase.empty()) {
    item["data"] = llvm::json::Object{{"phase", std::move(phase)}};
  }
  return item;
}

std::optional<llvm::json::Object>
make_code_action_for_diagnostic(
  const styio::ide::TextBuffer& buffer,
  const std::string& uri,
  const styio::ide::Diagnostic& diagnostic
) {
  if (diagnostic.code != styio::services::diagnostics::kServiceEditorSyntax) {
    return std::nullopt;
  }
  llvm::json::Array diagnostics;
  auto diagnostic_json = to_lsp_diagnostic_object(buffer, diagnostic);
  diagnostics.push_back(llvm::json::Value(std::move(diagnostic_json)));
  if (diagnostic.message == "unterminated block comment"
      && diagnostic.range.end <= buffer.size()) {
    llvm::json::Array edits;
    edits.push_back(llvm::json::Object{
      {"range", to_lsp_range(buffer, styio::ide::TextRange{diagnostic.range.end, diagnostic.range.end})},
      {"newText", " */"}});
    llvm::json::Object changes;
    changes[uri] = std::move(edits);
    return llvm::json::Object{
      {"title", "Close block comment"},
      {"kind", "quickfix"},
      {"diagnostics", std::move(diagnostics)},
      {"edit", llvm::json::Object{{"changes", std::move(changes)}}}};
  }
  if (diagnostic.message == "unterminated string literal"
      && diagnostic.range.end <= buffer.size()) {
    const auto& text = buffer.text();
    const bool at_line_boundary = diagnostic.range.end == text.size()
      || text[diagnostic.range.end] == '\n'
      || text[diagnostic.range.end] == '\r';
    if (at_line_boundary) {
      llvm::json::Array edits;
      edits.push_back(llvm::json::Object{
        {"range", to_lsp_range(buffer, styio::ide::TextRange{diagnostic.range.end, diagnostic.range.end})},
        {"newText", "\""}});
      llvm::json::Object changes;
      changes[uri] = std::move(edits);
      return llvm::json::Object{
        {"title", "Close string literal"},
        {"kind", "quickfix"},
        {"diagnostics", std::move(diagnostics)},
        {"edit", llvm::json::Object{{"changes", std::move(changes)}}}};
    }
  }
  const std::string unmatched_closing_prefix = "unmatched closing token ";
  if (diagnostic.message.rfind(unmatched_closing_prefix, 0) == 0
      && diagnostic.range.start < diagnostic.range.end
      && diagnostic.range.end <= buffer.size()) {
    const std::string closing_text = diagnostic.message.substr(unmatched_closing_prefix.size());
    const auto& text = buffer.text();
    const std::string range_text = text.substr(
      diagnostic.range.start,
      diagnostic.range.end - diagnostic.range.start);
    if (!closing_text.empty() && range_text == closing_text) {
      llvm::json::Array edits;
      edits.push_back(llvm::json::Object{
        {"range", to_lsp_range(buffer, diagnostic.range)},
        {"newText", ""}});
      llvm::json::Object changes;
      changes[uri] = std::move(edits);
      return llvm::json::Object{
        {"title", "Remove unmatched closing token"},
        {"kind", "quickfix"},
        {"diagnostics", std::move(diagnostics)},
        {"edit", llvm::json::Object{{"changes", std::move(changes)}}}};
    }
  }
  return llvm::json::Object{
    {"title", "No automatic fix available"},
    {"kind", "quickfix"},
    {"diagnostics", std::move(diagnostics)},
    {"disabled", llvm::json::Object{
      {"reason", "Editor-syntax diagnostics are intentionally not auto-fixable yet."}}}};
}

bool
text_ranges_intersect(
  const styio::ide::TextRange& lhs,
  const styio::ide::TextRange& rhs
) {
  return lhs.start < rhs.end && rhs.start < lhs.end;
}

std::optional<llvm::json::Object>
workspace_edit_for_rename(
  styio::ide::IdeService& service,
  std::vector<styio::ide::Location> locations,
  const std::string& new_name
) {
  if (new_name.empty()) {
    return std::nullopt;
  }

  std::unordered_set<std::string> seen_locations;
  std::vector<RenameEdit> edits;
  edits.reserve(locations.size());
  std::optional<std::string> original_text;

  for (const auto& location : locations) {
    if (location.range.end <= location.range.start) {
      return std::nullopt;
    }

    const std::string location_key = location.path + ":" + std::to_string(location.range.start) + ":" + std::to_string(location.range.end);
    if (!seen_locations.insert(location_key).second) {
      continue;
    }

    const std::string uri = styio::ide::uri_from_path(location.path);
    const auto snapshot = service.snapshot_for_uri(uri);
    if (location.range.end > snapshot->buffer.size()) {
      return std::nullopt;
    }

    const std::string location_text = snapshot->buffer.text().substr(location.range.start, location.range.length());
    if (location_text.empty()) {
      return std::nullopt;
    }
    if (!original_text.has_value()) {
      original_text = location_text;
    } else if (*original_text != location_text) {
      return std::nullopt;
    }

    edits.push_back(RenameEdit{
      uri,
      location.range,
      llvm::json::Object{
        {"range", to_lsp_range(snapshot->buffer, location.range)},
        {"newText", new_name}}});
  }

  if (edits.empty()) {
    return std::nullopt;
  }

  std::sort(
    edits.begin(),
    edits.end(),
    [](const RenameEdit& lhs, const RenameEdit& rhs)
    {
      if (lhs.uri != rhs.uri) {
        return lhs.uri < rhs.uri;
      }
      if (lhs.range.start != rhs.range.start) {
        return lhs.range.start > rhs.range.start;
      }
      return lhs.range.end > rhs.range.end;
    });

  llvm::json::Object changes;
  for (const auto& edit : edits) {
    auto& uri_edits = changes[edit.uri];
    if (uri_edits.getAsArray() == nullptr) {
      uri_edits = llvm::json::Array{};
    }
    auto* array = uri_edits.getAsArray();
    if (array == nullptr) {
      return std::nullopt;
    }
    array->push_back(llvm::json::Value(llvm::json::Object(edit.edit)));
  }

  return llvm::json::Object{{"changes", std::move(changes)}};
}

styio::ide::Position
internal_position_from_lsp_position(const styio::ide::TextBuffer& buffer, const llvm::json::Object& position) {
  const auto offset = offset_at_lsp_position(buffer, position);
  return buffer.position_at(offset.value_or(buffer.size()));
}

styio::ide::DocumentDelta
document_delta_from_lsp_changes(
  const styio::ide::TextBuffer& initial_buffer,
  const llvm::json::Array& changes
) {
  styio::ide::DocumentDelta delta;
  std::string working_text = initial_buffer.text();
  std::vector<styio::ide::TextEdit> edits;

  for (const auto& change_value : changes) {
    const auto* change = change_value.getAsObject();
    if (change == nullptr) {
      delta.requires_full_resync = true;
      delta.resync_reason = "malformed content change";
      break;
    }

    const std::string replacement = std::string(change->getString("text").value_or(""));
    const auto* range = change->getObject("range");
    if (range == nullptr) {
      delta.is_full_sync = true;
      working_text = replacement;
      edits.clear();
      continue;
    }

    const styio::ide::TextBuffer working_buffer(working_text);
    const auto text_range = range_from_lsp_range(working_buffer, *range);
    if (!text_range.has_value()) {
      delta.requires_full_resync = true;
      delta.resync_reason = "invalid incremental edit range";
      break;
    }

    styio::ide::TextEdit edit{*text_range, replacement};
    if (delta.is_full_sync) {
      working_text.replace(edit.range.start, edit.range.length(), edit.replacement);
      continue;
    }
    edits.push_back(edit);
    working_text.replace(edit.range.start, edit.range.length(), edit.replacement);
  }

  if (delta.requires_full_resync) {
    delta.is_full_sync = false;
    delta.full_text.clear();
    delta.edits.clear();
  } else if (delta.is_full_sync) {
    delta.full_text = std::move(working_text);
  } else {
    delta.edits = std::move(edits);
  }
  return delta;
}

llvm::json::Value
completion_kind_value(styio::ide::CompletionItemKind kind) {
  switch (kind) {
    case styio::ide::CompletionItemKind::Function:
      return static_cast<std::int64_t>(3);
    case styio::ide::CompletionItemKind::Type:
      return static_cast<std::int64_t>(7);
    case styio::ide::CompletionItemKind::Keyword:
      return static_cast<std::int64_t>(14);
    case styio::ide::CompletionItemKind::Snippet:
      return static_cast<std::int64_t>(15);
    case styio::ide::CompletionItemKind::Property:
      return static_cast<std::int64_t>(10);
    case styio::ide::CompletionItemKind::Module:
      return static_cast<std::int64_t>(9);
    case styio::ide::CompletionItemKind::Variable:
      return static_cast<std::int64_t>(6);
  }
  return static_cast<std::int64_t>(6);
}

llvm::json::Value
document_symbol_kind(styio::ide::SymbolKind kind) {
  switch (kind) {
    case styio::ide::SymbolKind::Function:
      return static_cast<std::int64_t>(12);
    case styio::ide::SymbolKind::Parameter:
      return static_cast<std::int64_t>(13);
    case styio::ide::SymbolKind::Builtin:
      return static_cast<std::int64_t>(14);
    case styio::ide::SymbolKind::Variable:
      return static_cast<std::int64_t>(13);
  }
  return static_cast<std::int64_t>(13);
}

void
write_message(std::ostream& output, const llvm::json::Object& payload) {
  const std::string body = llvm::formatv("{0}", llvm::json::Value(llvm::json::Object(payload))).str();
  output << "Content-Length: " << body.size() << "\r\n\r\n" << body;
  output.flush();
}

struct MessageReadResult
{
  MessageReadStatus status = MessageReadStatus::End;
  std::string body;
};

bool
discard_bytes(std::istream& input, std::size_t count) {
  char buffer[4096];
  while (count > 0) {
    const std::size_t chunk = std::min(count, sizeof(buffer));
    input.read(buffer, static_cast<std::streamsize>(chunk));
    if (static_cast<std::size_t>(input.gcount()) != chunk) {
      return false;
    }
    count -= chunk;
  }
  return true;
}

bool
ascii_starts_with_case_insensitive(std::string_view value, std::string_view prefix) {
  if (value.size() < prefix.size()) {
    return false;
  }
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    const unsigned char lhs = static_cast<unsigned char>(value[i]);
    const unsigned char rhs = static_cast<unsigned char>(prefix[i]);
    if (std::tolower(lhs) != std::tolower(rhs)) {
      return false;
    }
  }
  return true;
}

MessageReadResult
read_message_body(std::istream& input) {
  std::string line;
  std::size_t content_length = 0;
  bool saw_header = false;
  while (std::getline(input, line)) {
    saw_header = true;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    constexpr const char* k_header = "Content-Length:";
    if (ascii_starts_with_case_insensitive(line, k_header)) {
      std::string value = line.substr(std::char_traits<char>::length(k_header));
      value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }), value.end());
      if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        return MessageReadResult{MessageReadStatus::Skip, {}};
      }
      try {
        content_length = static_cast<std::size_t>(std::stoull(value));
      } catch (const std::exception&) {
        return MessageReadResult{MessageReadStatus::Skip, {}};
      }
      if (content_length > kMaxContentLength) {
        if (!discard_bytes(input, content_length)) {
          return MessageReadResult{MessageReadStatus::End, {}};
        }
        return MessageReadResult{MessageReadStatus::Skip, {}};
      }
    }
  }

  if (!saw_header) {
    return MessageReadResult{MessageReadStatus::End, {}};
  }

  if (content_length == 0) {
    return MessageReadResult{MessageReadStatus::Skip, {}};
  }

  std::string body(content_length, '\0');
  input.read(body.data(), static_cast<std::streamsize>(content_length));
  if (static_cast<std::size_t>(input.gcount()) != content_length) {
    return MessageReadResult{MessageReadStatus::End, {}};
  }
  return MessageReadResult{MessageReadStatus::Message, std::move(body)};
}

std::optional<std::uint64_t>
request_id_from_json(const llvm::json::Value& value) {
  if (auto integer = value.getAsInteger()) {
    if (*integer < 0) {
      return std::nullopt;
    }
    return static_cast<std::uint64_t>(*integer);
  }
  if (auto text = value.getAsString()) {
    const std::string request_id(*text);
    if (!request_id.empty() && std::all_of(request_id.begin(), request_id.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
      try {
        return static_cast<std::uint64_t>(std::stoull(request_id));
      } catch (const std::exception&) {
        return std::nullopt;
      }
    }
  }
  return std::nullopt;
}

std::optional<std::vector<std::string>>
watched_file_paths_from_params(const llvm::json::Object* params) {
  if (params == nullptr) {
    return std::nullopt;
  }

  const auto* changes = params->getArray("changes");
  if (changes == nullptr) {
    return std::nullopt;
  }

  std::vector<std::string> paths;
  std::unordered_set<std::string> seen_paths;
  for (const auto& change_value : *changes) {
    const auto* change = change_value.getAsObject();
    if (change == nullptr) {
      continue;
    }
    const std::string uri = std::string(change->getString("uri").value_or(""));
    if (uri.empty()) {
      continue;
    }
    const std::string path = styio::ide::path_from_uri(uri);
    if (seen_paths.insert(path).second) {
      paths.push_back(path);
    }
  }
  return paths;
}

std::vector<std::string>
workspace_folders_from_initialize_params(const llvm::json::Object* params) {
  std::vector<std::string> workspace_folders;
  if (params == nullptr) {
    return workspace_folders;
  }

  const auto* folders = params->getArray("workspaceFolders");
  if (folders == nullptr) {
    return workspace_folders;
  }

  for (const auto& folder_value : *folders) {
    const auto* folder = folder_value.getAsObject();
    if (folder == nullptr) {
      continue;
    }
    const std::string uri = std::string(folder->getString("uri").value_or(""));
    if (!uri.empty()) {
      workspace_folders.push_back(uri);
    }
  }

  return workspace_folders;
}

std::vector<std::string>
workspace_folders_ignored_for_selected_root(
  const std::vector<std::string>& workspace_folders,
  const std::string& selected_root_uri
) {
  std::vector<std::string> ignored;
  for (const auto& workspace_folder : workspace_folders) {
    if (!selected_root_uri.empty() && workspace_folder == selected_root_uri) {
      continue;
    }
    ignored.push_back(workspace_folder);
  }
  return ignored;
}

llvm::json::Array
strings_to_json_array(const std::vector<std::string>& values) {
  llvm::json::Array result;
  for (const auto& value : values) {
    result.push_back(value);
  }
  return result;
}

}  // namespace

llvm::json::Object
Server::make_diagnostic_notification(
  const std::string& uri,
  const styio::ide::TextBuffer& buffer,
  const std::vector<styio::ide::Diagnostic>& diagnostics
) {
  llvm::json::Array items;
  for (const auto& diagnostic : diagnostics) {
    items.push_back(to_lsp_diagnostic_object(buffer, diagnostic));
  }

  return llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/publishDiagnostics"},
    {"params", llvm::json::Object{{"uri", uri}, {"diagnostics", std::move(items)}}}};
}

llvm::json::Object
Server::make_initialize_workspace_state() const {
  return llvm::json::Object{
    {"mode", "single"},
    {"requestedRootUri", initialize_requested_root_uri_},
    {"selectedRootUri", initialize_selected_root_uri_},
    {"workspaceFolders", strings_to_json_array(initialize_workspace_folders_)},
    {"ignoredWorkspaceFolders", strings_to_json_array(initialize_ignored_workspace_folders_)}};
}

std::vector<OutboundMessage>
Server::handle(llvm::json::Object request) {
  std::vector<OutboundMessage> output;
  const std::string method = std::string(request.getString("method").value_or(""));
  const llvm::json::Value* id = request.get("id");
  const auto numeric_id = id == nullptr ? std::nullopt : request_id_from_json(*id);
  const llvm::json::Object* params = request.getObject("params");

  auto respond = [&](llvm::json::Value result)
  {
    if (id == nullptr) {
      return;
    }
    output.push_back(OutboundMessage{
      llvm::json::Object{
        {"jsonrpc", "2.0"},
        {"id", *id},
        {"result", std::move(result)}},
      false});
  };

  if (method == "initialize") {
    initialize_requested_root_uri_.clear();
    initialize_selected_root_uri_.clear();
    initialize_workspace_folders_.clear();
    initialize_ignored_workspace_folders_.clear();

    if (params != nullptr) {
      initialize_requested_root_uri_ = std::string(params->getString("rootUri").value_or(""));
      initialize_workspace_folders_ = workspace_folders_from_initialize_params(params);
      initialize_selected_root_uri_ = !initialize_requested_root_uri_.empty()
        ? initialize_requested_root_uri_
        : (initialize_workspace_folders_.empty() ? std::string{} : initialize_workspace_folders_.front());
      initialize_ignored_workspace_folders_ = workspace_folders_ignored_for_selected_root(
        initialize_workspace_folders_,
        initialize_selected_root_uri_);
    }

    service_.initialize(initialize_selected_root_uri_);

    llvm::json::Array semantic_token_types{
      "namespace", "type", "class", "enum", "interface", "struct", "typeParameter", "parameter",
      "variable", "property", "enumMember", "event", "function", "method", "macro", "keyword",
      "modifier", "comment", "string", "number", "operator"};

    llvm::json::Object capabilities{
      {"textDocumentSync", llvm::json::Object{{"openClose", true}, {"change", 2}}},
      {"completionProvider", llvm::json::Object{}},
      {"hoverProvider", true},
      {"codeActionProvider", true},
      {"definitionProvider", true},
      {"referencesProvider", true},
      {"renameProvider", true},
      {"inlayHintProvider", true},
      {"documentSymbolProvider", true},
      {"workspaceSymbolProvider", true},
      {"workspace", llvm::json::Object{
        {"workspaceFolders", llvm::json::Object{{"supported", false}}}}},
      {"semanticTokensProvider", llvm::json::Object{
        {"legend", llvm::json::Object{
          {"tokenTypes", std::move(semantic_token_types)},
          {"tokenModifiers", llvm::json::Array{}}}},
        {"full", true}}}};

    llvm::json::Object result{
      {"capabilities", std::move(capabilities)},
      {"experimental", llvm::json::Object{
        {"styio", llvm::json::Object{
          {"workspaceState", make_initialize_workspace_state()}}}}}};

    if (id != nullptr) {
      output.push_back(OutboundMessage{
        llvm::json::Object{
          {"jsonrpc", "2.0"},
          {"id", *id},
          {"result", std::move(result)}},
        false});
    }
    return output;
  }

  if (method == "initialized") {
    return output;
  }

  if (method == "$/cancelRequest") {
    if (params != nullptr) {
      const llvm::json::Value* cancel_id = params->get("id");
      if (cancel_id != nullptr) {
        if (const auto numeric_cancel_id = request_id_from_json(*cancel_id); numeric_cancel_id.has_value()) {
          service_.cancel_request(*numeric_cancel_id);
        }
      }
    }
    return output;
  }

  if (method == "workspace/didChangeWatchedFiles") {
    const auto watched_paths = watched_file_paths_from_params(params);
    if (watched_paths.has_value()) {
      if (watched_paths->empty()) {
        service_.schedule_background_index_refresh();
      }
      else {
        service_.schedule_background_index_refresh_for_paths(*watched_paths);
      }
    }
    return output;
  }

  if (params == nullptr) {
    respond(llvm::json::Value(nullptr));
    return output;
  }

  if (method == "textDocument/didOpen") {
    const auto* text_document = params->getObject("textDocument");
    if (text_document != nullptr) {
      const std::string uri = std::string(text_document->getString("uri").value_or(""));
      const std::string text = std::string(text_document->getString("text").value_or(""));
      const auto diagnostics = service_.did_open(
        uri,
        text,
        static_cast<styio::ide::DocumentVersion>(text_document->getInteger("version").value_or(0)));
      diagnostics_cache_[uri] = diagnostics;
      const auto snapshot = service_.snapshot_for_uri(uri);
      output.push_back(OutboundMessage{make_diagnostic_notification(uri, snapshot->buffer, diagnostics), true});
    }
    return output;
  }

  if (method == "textDocument/didChange") {
    const auto* text_document = params->getObject("textDocument");
    const auto* changes = params->getArray("contentChanges");
    if (text_document != nullptr && changes != nullptr && !changes->empty()) {
      const std::string uri = std::string(text_document->getString("uri").value_or(""));
      const auto current_snapshot = service_.snapshot_for_uri(uri);
      const styio::ide::DocumentDelta delta = document_delta_from_lsp_changes(current_snapshot->buffer, *changes);
      const auto diagnostics = service_.did_change(
        uri,
        delta,
        static_cast<styio::ide::DocumentVersion>(text_document->getInteger("version").value_or(0)));
      diagnostics_cache_[uri] = diagnostics;
      const auto snapshot = service_.snapshot_for_uri(uri);
      output.push_back(OutboundMessage{make_diagnostic_notification(uri, snapshot->buffer, diagnostics), true});
    }
    return output;
  }

  if (method == "textDocument/didClose") {
    const auto* text_document = params->getObject("textDocument");
    if (text_document != nullptr) {
      const std::string uri = std::string(text_document->getString("uri").value_or(""));
      service_.did_close(uri);
      diagnostics_cache_.erase(uri);
    }
    return output;
  }

  if (method == "textDocument/codeAction") {
    if (service_.pending_semantic_diagnostic_count() != 0 || service_.pending_background_task_count() != 0) {
      respond(llvm::json::Array{});
      return output;
    }

    const auto* text_document = params->getObject("textDocument");
    const auto* range = params->getObject("range");
    llvm::json::Array actions;
    llvm::json::Array disabled_actions;
    if (text_document != nullptr && range != nullptr) {
      const std::string uri = std::string(text_document->getString("uri").value_or(""));
      const auto snapshot = service_.snapshot_for_uri(uri);
      if (snapshot->is_open) {
        const auto request_range = range_from_lsp_range(snapshot->buffer, *range);
        if (request_range.has_value()) {
          const auto it = diagnostics_cache_.find(uri);
          if (it != diagnostics_cache_.end()) {
            for (const auto& diagnostic : it->second) {
              if (!text_ranges_intersect(diagnostic.range, *request_range)) {
                continue;
              }
              if (auto action = make_code_action_for_diagnostic(snapshot->buffer, uri, diagnostic)) {
                llvm::json::Array& target_actions =
                  (action->getObject("edit") != nullptr || action->getObject("command") != nullptr)
                  ? actions
                  : disabled_actions;
                target_actions.push_back(llvm::json::Value(std::move(*action)));
              }
            }
          }
        }
      }
    }
    for (auto& action : disabled_actions) {
      actions.push_back(std::move(action));
    }
    respond(std::move(actions));
    return output;
  }

  if (method == "textDocument/completion") {
    const auto* text_document = params->getObject("textDocument");
    const auto* position = params->getObject("position");
    llvm::json::Array items;
    if (text_document != nullptr && position != nullptr) {
      const std::string uri = std::string(text_document->getString("uri").value_or(""));
      const auto snapshot = service_.snapshot_for_uri(uri);
      const auto ticket = numeric_id.has_value()
        ? service_.begin_foreground_request(uri, styio::ide::RuntimeRequestKind::Completion, *numeric_id)
        : service_.begin_foreground_request(uri, styio::ide::RuntimeRequestKind::Completion);
      for (const auto& item : service_.completion(
             ticket,
             internal_position_from_lsp_position(snapshot->buffer, *position))) {
        items.push_back(llvm::json::Object{
          {"label", item.label},
          {"kind", completion_kind_value(item.kind)},
          {"insertText", item.insert_text},
          {"detail", item.detail},
          {"sortText", std::to_string(999999 - item.sort_score)}});
      }
    }
    respond(std::move(items));
    return output;
  }

  if (method == "textDocument/hover") {
    const auto* text_document = params->getObject("textDocument");
    const auto* position = params->getObject("position");
    if (text_document != nullptr && position != nullptr) {
      const std::string uri = std::string(text_document->getString("uri").value_or(""));
      const auto snapshot = service_.snapshot_for_uri(uri);
      const auto ticket = numeric_id.has_value()
        ? service_.begin_foreground_request(uri, styio::ide::RuntimeRequestKind::Hover, *numeric_id)
        : service_.begin_foreground_request(uri, styio::ide::RuntimeRequestKind::Hover);
      const auto hover = service_.hover(
        ticket,
        internal_position_from_lsp_position(snapshot->buffer, *position));
      if (hover.has_value()) {
        llvm::json::Object result{{"contents", hover->contents}};
        if (hover->range.has_value()) {
          result["range"] = to_lsp_range(snapshot->buffer, *hover->range);
        }
        respond(std::move(result));
      } else {
        respond(llvm::json::Value(nullptr));
      }
    } else {
      respond(llvm::json::Value(nullptr));
    }
    return output;
  }

  if (method == "textDocument/definition" || method == "textDocument/references") {
    const auto* text_document = params->getObject("textDocument");
    const auto* position = params->getObject("position");
    llvm::json::Array locations;
    if (text_document != nullptr && position != nullptr) {
      const std::string uri = std::string(text_document->getString("uri").value_or(""));
      const auto snapshot = service_.snapshot_for_uri(uri);
      const styio::ide::Position pos = internal_position_from_lsp_position(snapshot->buffer, *position);
      const auto ticket = numeric_id.has_value()
        ? service_.begin_foreground_request(
            uri,
            method == "textDocument/definition"
              ? styio::ide::RuntimeRequestKind::Definition
              : styio::ide::RuntimeRequestKind::References,
            *numeric_id)
        : service_.begin_foreground_request(
            uri,
            method == "textDocument/definition"
              ? styio::ide::RuntimeRequestKind::Definition
              : styio::ide::RuntimeRequestKind::References);
      const auto results = method == "textDocument/definition"
        ? service_.definition(ticket, pos)
        : service_.references(ticket, pos);
      for (const auto& location : results) {
        const auto snapshot = service_.snapshot_for_uri(styio::ide::uri_from_path(location.path));
        locations.push_back(llvm::json::Object{
          {"uri", styio::ide::uri_from_path(location.path)},
          {"range", to_lsp_range(snapshot->buffer, location.range)}});
      }
    }
    respond(std::move(locations));
    return output;
  }

  if (method == "textDocument/rename") {
    if (service_.pending_semantic_diagnostic_count() != 0 || service_.pending_background_task_count() != 0) {
      respond(llvm::json::Value(nullptr));
      return output;
    }

    const auto* text_document = params->getObject("textDocument");
    const auto* position = params->getObject("position");
    const std::string new_name = std::string(params->getString("newName").value_or(""));
    if (text_document != nullptr && position != nullptr && !new_name.empty()) {
      const std::string uri = std::string(text_document->getString("uri").value_or(""));
      const auto snapshot = service_.snapshot_for_uri(uri);
      const styio::ide::Position pos = internal_position_from_lsp_position(snapshot->buffer, *position);
      const auto ticket = numeric_id.has_value()
        ? service_.begin_foreground_request(uri, styio::ide::RuntimeRequestKind::Definition, *numeric_id)
        : service_.begin_foreground_request(uri, styio::ide::RuntimeRequestKind::Definition);
      std::vector<styio::ide::Location> locations = service_.definition(ticket, pos);
      if (locations.empty()) {
        respond(llvm::json::Value(nullptr));
        return output;
      }
      const auto references = service_.references(ticket, pos);
      locations.insert(locations.end(), references.begin(), references.end());
      auto workspace_edit = workspace_edit_for_rename(service_, std::move(locations), new_name);
      if (workspace_edit.has_value()) {
        respond(llvm::json::Value(std::move(*workspace_edit)));
      } else {
        respond(llvm::json::Value(nullptr));
      }
    } else {
      respond(llvm::json::Value(nullptr));
    }
    return output;
  }

  if (method == "textDocument/inlayHint") {
    if (service_.pending_semantic_diagnostic_count() != 0 || service_.pending_background_task_count() != 0) {
      respond(llvm::json::Array{});
      return output;
    }

    const auto* text_document = params->getObject("textDocument");
    const auto* range = params->getObject("range");
    llvm::json::Array hints;
    if (text_document != nullptr && range != nullptr) {
      const std::string uri = std::string(text_document->getString("uri").value_or(""));
      const auto snapshot = service_.snapshot_for_uri(uri);
      const auto request_range = range_from_lsp_range(snapshot->buffer, *range);
      if (request_range.has_value()) {
        for (auto& hint : parameter_inlay_hints_for_range(service_, uri, *request_range)) {
          hints.push_back(std::move(hint));
        }
      }
    }
    respond(std::move(hints));
    return output;
  }

  if (method == "textDocument/documentSymbol") {
    const auto* text_document = params->getObject("textDocument");
    llvm::json::Array symbols;
    if (text_document != nullptr) {
      const std::string uri = std::string(text_document->getString("uri").value_or(""));
      const auto snapshot = service_.snapshot_for_uri(uri);
      for (const auto& symbol : service_.document_symbols(uri)) {
        symbols.push_back(llvm::json::Object{
          {"name", symbol.name},
          {"kind", document_symbol_kind(symbol.kind)},
          {"detail", symbol.detail},
          {"range", to_lsp_range(snapshot->buffer, symbol.range)},
          {"selectionRange", to_lsp_range(snapshot->buffer, symbol.selection_range)}});
      }
    }
    respond(std::move(symbols));
    return output;
  }

  if (method == "workspace/symbol") {
    llvm::json::Array symbols;
    const std::string query = std::string(params->getString("query").value_or(""));
    for (const auto& symbol : service_.workspace_symbols(query)) {
      const auto snapshot = service_.snapshot_for_uri(styio::ide::uri_from_path(symbol.path));
      symbols.push_back(llvm::json::Object{
        {"name", symbol.name},
        {"kind", document_symbol_kind(symbol.kind)},
        {"location", llvm::json::Object{
           {"uri", styio::ide::uri_from_path(symbol.path)},
           {"range", to_lsp_range(snapshot->buffer, symbol.range)}}}});
    }
    respond(std::move(symbols));
    return output;
  }

  if (method == "textDocument/semanticTokens/full") {
    const auto* text_document = params->getObject("textDocument");
    llvm::json::Array data;
    if (text_document != nullptr) {
      for (std::uint32_t value : service_.semantic_tokens(std::string(text_document->getString("uri").value_or("")))) {
        data.push_back(static_cast<std::int64_t>(value));
      }
    }
    respond(llvm::json::Object{{"data", std::move(data)}});
    return output;
  }

  respond(llvm::json::Value(nullptr));
  return output;
}

std::vector<OutboundMessage>
Server::drain_runtime() {
  return drain_runtime(static_cast<std::size_t>(-1));
}

std::vector<OutboundMessage>
Server::drain_runtime(std::size_t max_documents) {
  std::vector<OutboundMessage> output;
  for (auto publication : service_.drain_semantic_diagnostics(max_documents)) {
    if (publication.snapshot == nullptr) {
      continue;
    }
    const std::string uri = styio::ide::uri_from_path(publication.snapshot->path);
    diagnostics_cache_[uri] = publication.diagnostics;
    output.push_back(OutboundMessage{
      make_diagnostic_notification(
        uri,
        publication.snapshot->buffer,
        publication.diagnostics),
      true});
  }
  return output;
}

const styio::ide::RuntimeCounters&
Server::runtime_counters() const {
  return service_.runtime_counters();
}

void
Server::run(std::istream& input, std::ostream& output) {
  while (true) {
    const auto message = read_message_body(input);
    if (message.status == MessageReadStatus::End) {
      break;
    }
    if (message.status == MessageReadStatus::Skip) {
      continue;
    }

    llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(message.body);
    if (!parsed) {
      continue;
    }

    auto* object = parsed->getAsObject();
    if (object == nullptr) {
      continue;
    }

    for (const auto& message : handle(std::move(*object))) {
      write_message(output, message.payload);
    }

    for (const auto& message : drain_runtime(kRuntimeDrainBudgetPerLoop)) {
      write_message(output, message.payload);
    }

    if (service_.pending_background_task_count() > 0 &&
        service_.pending_semantic_diagnostic_count() == 0) {
      service_.run_background_tasks(kBackgroundWorkBudgetPerLoop);
    }
  }
}

}  // namespace styio::lsp
