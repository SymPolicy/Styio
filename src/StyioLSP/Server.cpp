#include "Server.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <sstream>

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
    if (line.rfind(k_header, 0) == 0) {
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

}  // namespace

llvm::json::Object
Server::make_diagnostic_notification(
  const std::string& uri,
  const styio::ide::TextBuffer& buffer,
  const std::vector<styio::ide::Diagnostic>& diagnostics
) {
  llvm::json::Array items;
  for (const auto& diagnostic : diagnostics) {
    items.push_back(llvm::json::Object{
      {"range", to_lsp_range(buffer, diagnostic.range)},
      {"severity", static_cast<std::int64_t>(diagnostic.severity)},
      {"source", diagnostic.source},
      {"message", diagnostic.message}});
  }

  return llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/publishDiagnostics"},
    {"params", llvm::json::Object{{"uri", uri}, {"diagnostics", std::move(items)}}}};
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
    if (params != nullptr) {
      service_.initialize(std::string(params->getString("rootUri").value_or("")));
    }

    llvm::json::Array semantic_token_types{
      "namespace", "type", "class", "enum", "interface", "struct", "typeParameter", "parameter",
      "variable", "property", "enumMember", "event", "function", "method", "macro", "keyword",
      "modifier", "comment", "string", "number", "operator"};

    respond(llvm::json::Object{
      {"capabilities", llvm::json::Object{
         {"textDocumentSync", llvm::json::Object{{"openClose", true}, {"change", 2}}},
         {"completionProvider", llvm::json::Object{}},
         {"hoverProvider", true},
         {"definitionProvider", true},
         {"referencesProvider", true},
         {"documentSymbolProvider", true},
         {"workspaceSymbolProvider", true},
         {"semanticTokensProvider", llvm::json::Object{
            {"legend", llvm::json::Object{{"tokenTypes", std::move(semantic_token_types)}, {"tokenModifiers", llvm::json::Array{}}}},
            {"full", true}}}}}});
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
    service_.schedule_background_index_refresh();
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
      const auto snapshot = service_.snapshot_for_uri(uri);
      output.push_back(OutboundMessage{make_diagnostic_notification(uri, snapshot->buffer, diagnostics), true});
    }
    return output;
  }

  if (method == "textDocument/didClose") {
    const auto* text_document = params->getObject("textDocument");
    if (text_document != nullptr) {
      service_.did_close(std::string(text_document->getString("uri").value_or("")));
    }
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
    output.push_back(OutboundMessage{
      make_diagnostic_notification(
        styio::ide::uri_from_path(publication.snapshot->path),
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
