#include "Server.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace styio::lsp {

namespace {

llvm::json::Object
to_lsp_range(const styio::ide::TextBuffer& buffer, styio::ide::TextRange range) {
  const styio::ide::Position start = buffer.position_at(range.start);
  const styio::ide::Position end = buffer.position_at(range.end);
  return llvm::json::Object{
    {"start", llvm::json::Object{{"line", static_cast<std::int64_t>(start.line)}, {"character", static_cast<std::int64_t>(start.character)}}},
    {"end", llvm::json::Object{{"line", static_cast<std::int64_t>(end.line)}, {"character", static_cast<std::int64_t>(end.character)}}}};
}

styio::ide::Position
from_lsp_position(const llvm::json::Object& position) {
  return styio::ide::Position{
    static_cast<std::size_t>(position.getInteger("line").value_or(0)),
    static_cast<std::size_t>(position.getInteger("character").value_or(0))};
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

std::optional<std::string>
read_message_body(std::istream& input) {
  std::string line;
  std::size_t content_length = 0;
  while (std::getline(input, line)) {
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
      content_length = static_cast<std::size_t>(std::stoul(value));
    }
  }

  if (content_length == 0) {
    return std::nullopt;
  }

  std::string body(content_length, '\0');
  input.read(body.data(), static_cast<std::streamsize>(content_length));
  return body;
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
         {"textDocumentSync", llvm::json::Object{{"openClose", true}, {"change", 1}}},
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
      const auto* first_change = (*changes)[0].getAsObject();
      const std::string text = first_change != nullptr ? std::string(first_change->getString("text").value_or("")) : "";
      const auto diagnostics = service_.did_change(
        uri,
        text,
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
      for (const auto& item : service_.completion(
             std::string(text_document->getString("uri").value_or("")),
             from_lsp_position(*position))) {
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
      const auto hover = service_.hover(
        std::string(text_document->getString("uri").value_or("")),
        from_lsp_position(*position));
      if (hover.has_value()) {
        llvm::json::Object result{{"contents", hover->contents}};
        if (hover->range.has_value()) {
          const auto snapshot = service_.snapshot_for_uri(std::string(text_document->getString("uri").value_or("")));
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
      const styio::ide::Position pos = from_lsp_position(*position);
      const auto results = method == "textDocument/definition"
        ? service_.definition(uri, pos)
        : service_.references(uri, pos);
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

void
Server::run(std::istream& input, std::ostream& output) {
  while (true) {
    const auto body = read_message_body(input);
    if (!body.has_value()) {
      break;
    }

    llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(*body);
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
  }
}

}  // namespace styio::lsp
