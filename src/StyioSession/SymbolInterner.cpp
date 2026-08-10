#include "SymbolInterner.hpp"

namespace styio::session {

SymbolInterner::SymbolInterner() {
  // Reserve id 0 as invalid
  symbols_.emplace_back("<invalid>");
}

SymbolInterner::~SymbolInterner() = default;

SymbolId SymbolInterner::intern(std::string_view spelling) {
  ++intern_calls_;
  ++lookup_probes_;
  auto it = map_.find(spelling);
  if (it != map_.end()) {
    return it->second;
  }
  // symbols_[0] is the invalid sentinel
  SymbolId id = static_cast<SymbolId>(symbols_.size());
  symbols_.emplace_back(spelling);
  // Re-obtain string_view into the stable deque storage
  std::string_view stable = symbols_.back();
  map_.emplace(stable, id);
  ++insertions_;
  return id;
}

SymbolId SymbolInterner::lookup(std::string_view spelling) const {
  ++lookup_probes_;
  auto it = map_.find(spelling);
  return it != map_.end() ? it->second : kInvalidSymbolId;
}

std::string_view SymbolInterner::resolve(SymbolId id) const {
  if (id == kInvalidSymbolId || id >= symbols_.size()) {
    return symbols_[0]; // "<invalid>"
  }
  return symbols_[id];
}

bool SymbolInterner::contains(std::string_view spelling) const {
  return map_.find(spelling) != map_.end();
}

void SymbolInterner::reserve(std::size_t expected) {
  map_.reserve(expected);
}

} // namespace styio::session
