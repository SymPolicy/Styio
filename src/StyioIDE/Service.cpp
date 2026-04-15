#include "Service.hpp"

namespace styio::ide {

IdeService::IdeService() :
    semdb_(vfs_, project_) {
}

void
IdeService::initialize(const std::string& root_uri) {
  project_.set_root(path_from_uri(root_uri));
  semdb_.configure_cache_root(project_.cache_root());
  semdb_.index_workspace();
}

std::vector<Diagnostic>
IdeService::did_open(const std::string& uri, const std::string& text, DocumentVersion version) {
  const std::string path = path_from_uri(uri);
  vfs_.open(path, text, version);
  return semdb_.diagnostics_for(path);
}

std::vector<Diagnostic>
IdeService::did_change(const std::string& uri, const std::string& text, DocumentVersion version) {
  const std::string path = path_from_uri(uri);
  vfs_.update(path, text, version);
  return semdb_.diagnostics_for(path);
}

void
IdeService::did_close(const std::string& uri) {
  const std::string path = path_from_uri(uri);
  vfs_.close(path);
  semdb_.drop_open_file(path);
}

std::vector<CompletionItem>
IdeService::completion(const std::string& uri, Position position) {
  const std::string path = path_from_uri(uri);
  auto snapshot = vfs_.snapshot_for(path);
  return semdb_.complete_at(path, snapshot->buffer.offset_at(position));
}

std::optional<HoverResult>
IdeService::hover(const std::string& uri, Position position) {
  const std::string path = path_from_uri(uri);
  auto snapshot = vfs_.snapshot_for(path);
  return semdb_.hover_at(path, snapshot->buffer.offset_at(position));
}

std::vector<Location>
IdeService::definition(const std::string& uri, Position position) {
  const std::string path = path_from_uri(uri);
  auto snapshot = vfs_.snapshot_for(path);
  return semdb_.definition_at(path, snapshot->buffer.offset_at(position));
}

std::vector<Location>
IdeService::references(const std::string& uri, Position position) {
  const std::string path = path_from_uri(uri);
  auto snapshot = vfs_.snapshot_for(path);
  return semdb_.references_of(path, snapshot->buffer.offset_at(position));
}

std::vector<DocumentSymbol>
IdeService::document_symbols(const std::string& uri) {
  return semdb_.document_symbols(path_from_uri(uri));
}

std::vector<IndexedSymbol>
IdeService::workspace_symbols(const std::string& query) {
  return semdb_.workspace_symbols(query);
}

std::vector<std::uint32_t>
IdeService::semantic_tokens(const std::string& uri) {
  return semdb_.semantic_tokens_for(path_from_uri(uri));
}

std::shared_ptr<const DocumentSnapshot>
IdeService::snapshot_for_uri(const std::string& uri) {
  return vfs_.snapshot_for(path_from_uri(uri));
}

}  // namespace styio::ide
