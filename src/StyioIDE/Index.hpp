#pragma once

#ifndef STYIO_IDE_INDEX_HPP_
#define STYIO_IDE_INDEX_HPP_

#include <string>
#include <unordered_map>
#include <vector>

#include "Common.hpp"
#include "HIR.hpp"

namespace styio::ide {

struct IndexedSymbol
{
  std::string path;
  std::string name;
  SymbolKind kind = SymbolKind::Variable;
  TextRange range;
  TextRange selection_range;
  std::string detail;
};

struct IndexedReference
{
  std::string path;
  std::string name;
  TextRange range;
};

class OpenFileIndex
{
private:
  std::unordered_map<std::string, std::vector<IndexedSymbol>> symbols_by_file_;
  std::unordered_map<std::string, std::vector<IndexedReference>> references_by_file_;

public:
  void update(const std::string& path, const HirModule& module);
  void erase(const std::string& path);

  std::vector<IndexedSymbol> query_symbols(const std::string& query) const;
  std::vector<IndexedReference> query_references(const std::string& name) const;
};

class BackgroundIndex
{
private:
  std::unordered_map<std::string, std::vector<IndexedSymbol>> symbols_by_file_;
  std::unordered_map<std::string, std::vector<IndexedReference>> references_by_file_;

public:
  void update(const std::string& path, const HirModule& module);
  std::vector<IndexedSymbol> query_symbols(const std::string& query) const;
  std::vector<IndexedReference> query_references(const std::string& name) const;
};

class PersistentIndex
{
private:
  std::string cache_root_;

public:
  explicit PersistentIndex(std::string cache_root = "");

  void set_cache_root(std::string cache_root);
  void save_symbols(const std::vector<IndexedSymbol>& symbols) const;
  std::vector<IndexedSymbol> load_symbols() const;
};

}  // namespace styio::ide

#endif
