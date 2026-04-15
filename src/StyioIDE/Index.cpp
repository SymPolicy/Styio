#include "Index.hpp"

#include <filesystem>
#include <fstream>

#include "llvm/Support/JSON.h"

namespace styio::ide {

namespace {

std::vector<IndexedSymbol>
query_symbols_impl(
  const std::unordered_map<std::string, std::vector<IndexedSymbol>>& symbols_by_file,
  const std::string& query
) {
  std::vector<IndexedSymbol> results;
  for (const auto& file_entry : symbols_by_file) {
    for (const auto& symbol : file_entry.second) {
      if (query.empty() || symbol.name.find(query) != std::string::npos) {
        results.push_back(symbol);
      }
    }
  }
  return results;
}

std::vector<IndexedReference>
query_references_impl(
  const std::unordered_map<std::string, std::vector<IndexedReference>>& references_by_file,
  const std::string& name
) {
  std::vector<IndexedReference> results;
  for (const auto& file_entry : references_by_file) {
    for (const auto& reference : file_entry.second) {
      if (reference.name == name) {
        results.push_back(reference);
      }
    }
  }
  return results;
}

}  // namespace

void
OpenFileIndex::update(const std::string& path, const HirModule& module) {
  std::vector<IndexedSymbol> symbols;
  for (const auto& symbol : module.symbols) {
    symbols.push_back(IndexedSymbol{
      path,
      symbol.name,
      symbol.kind,
      symbol.full_range,
      symbol.name_range,
      symbol.detail.empty() ? symbol.type_name : symbol.detail});
  }
  symbols_by_file_[path] = std::move(symbols);

  std::vector<IndexedReference> references;
  for (const auto& reference : module.references) {
    references.push_back(IndexedReference{path, reference.name, reference.range});
  }
  references_by_file_[path] = std::move(references);
}

void
OpenFileIndex::erase(const std::string& path) {
  symbols_by_file_.erase(path);
  references_by_file_.erase(path);
}

std::vector<IndexedSymbol>
OpenFileIndex::query_symbols(const std::string& query) const {
  return query_symbols_impl(symbols_by_file_, query);
}

std::vector<IndexedReference>
OpenFileIndex::query_references(const std::string& name) const {
  return query_references_impl(references_by_file_, name);
}

void
BackgroundIndex::update(const std::string& path, const HirModule& module) {
  std::vector<IndexedSymbol> symbols;
  for (const auto& symbol : module.symbols) {
    symbols.push_back(IndexedSymbol{
      path,
      symbol.name,
      symbol.kind,
      symbol.full_range,
      symbol.name_range,
      symbol.detail.empty() ? symbol.type_name : symbol.detail});
  }
  symbols_by_file_[path] = std::move(symbols);

  std::vector<IndexedReference> references;
  for (const auto& reference : module.references) {
    references.push_back(IndexedReference{path, reference.name, reference.range});
  }
  references_by_file_[path] = std::move(references);
}

std::vector<IndexedSymbol>
BackgroundIndex::query_symbols(const std::string& query) const {
  return query_symbols_impl(symbols_by_file_, query);
}

std::vector<IndexedReference>
BackgroundIndex::query_references(const std::string& name) const {
  return query_references_impl(references_by_file_, name);
}

PersistentIndex::PersistentIndex(std::string cache_root) :
    cache_root_(std::move(cache_root)) {
}

void
PersistentIndex::set_cache_root(std::string cache_root) {
  cache_root_ = std::move(cache_root);
}

void
PersistentIndex::save_symbols(const std::vector<IndexedSymbol>& symbols) const {
  if (cache_root_.empty()) {
    return;
  }

  std::filesystem::create_directories(cache_root_);
  llvm::json::Array items;
  for (const auto& symbol : symbols) {
    items.push_back(llvm::json::Object{
      {"path", symbol.path},
      {"name", symbol.name},
      {"kind", to_string(symbol.kind)},
      {"range_start", static_cast<std::int64_t>(symbol.range.start)},
      {"range_end", static_cast<std::int64_t>(symbol.range.end)},
      {"selection_start", static_cast<std::int64_t>(symbol.selection_range.start)},
      {"selection_end", static_cast<std::int64_t>(symbol.selection_range.end)},
      {"detail", symbol.detail}});
  }

  std::ofstream output(std::filesystem::path(cache_root_) / "symbols.json");
  output << llvm::formatv("{0:2}", llvm::json::Value(std::move(items))).str();
}

std::vector<IndexedSymbol>
PersistentIndex::load_symbols() const {
  std::vector<IndexedSymbol> symbols;
  if (cache_root_.empty()) {
    return symbols;
  }

  std::ifstream input(std::filesystem::path(cache_root_) / "symbols.json");
  if (!input) {
    return symbols;
  }

  std::string payload((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(payload);
  if (!parsed) {
    return symbols;
  }

  const auto* array = parsed->getAsArray();
  if (array == nullptr) {
    return symbols;
  }

  for (const auto& value : *array) {
    const auto* object = value.getAsObject();
    if (object == nullptr) {
      continue;
    }
    IndexedSymbol symbol;
    if (auto path = object->getString("path")) {
      symbol.path = std::string(*path);
    }
    if (auto name = object->getString("name")) {
      symbol.name = std::string(*name);
    }
    if (auto detail = object->getString("detail")) {
      symbol.detail = std::string(*detail);
    }
    symbol.range.start = static_cast<std::size_t>(object->getInteger("range_start").value_or(0));
    symbol.range.end = static_cast<std::size_t>(object->getInteger("range_end").value_or(0));
    symbol.selection_range.start = static_cast<std::size_t>(object->getInteger("selection_start").value_or(0));
    symbol.selection_range.end = static_cast<std::size_t>(object->getInteger("selection_end").value_or(0));
    symbols.push_back(std::move(symbol));
  }

  return symbols;
}

}  // namespace styio::ide
