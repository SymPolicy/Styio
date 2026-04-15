#pragma once

#ifndef STYIO_IDE_SERVICE_HPP_
#define STYIO_IDE_SERVICE_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "Common.hpp"
#include "Index.hpp"
#include "Project.hpp"
#include "SemDB.hpp"
#include "VFS.hpp"

namespace styio::ide {

class IdeService
{
private:
  VirtualFileSystem vfs_;
  Project project_;
  SemanticDB semdb_;

public:
  IdeService();

  void initialize(const std::string& root_uri);

  std::vector<Diagnostic> did_open(const std::string& uri, const std::string& text, DocumentVersion version);
  std::vector<Diagnostic> did_change(const std::string& uri, const std::string& text, DocumentVersion version);
  void did_close(const std::string& uri);

  std::vector<CompletionItem> completion(const std::string& uri, Position position);
  std::optional<HoverResult> hover(const std::string& uri, Position position);
  std::vector<Location> definition(const std::string& uri, Position position);
  std::vector<Location> references(const std::string& uri, Position position);
  std::vector<DocumentSymbol> document_symbols(const std::string& uri);
  std::vector<IndexedSymbol> workspace_symbols(const std::string& query);
  std::vector<std::uint32_t> semantic_tokens(const std::string& uri);
  std::shared_ptr<const DocumentSnapshot> snapshot_for_uri(const std::string& uri);
};

}  // namespace styio::ide

#endif
