#pragma once
#ifndef STYIO_SESSION_SYMBOL_INTERNER_HPP_
#define STYIO_SESSION_SYMBOL_INTERNER_HPP_

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

namespace styio::session {

using SymbolId = std::uint32_t;
inline constexpr SymbolId kInvalidSymbolId = 0;

/// Session-local string interner — each distinct spelling is stored once.
/// Lookups use transparent hashing on string_view (no std::string temporary).
///
/// Thread safety: NOT thread-safe. Each CompilationSession owns one instance;
/// access is single-threaded per session.
class SymbolInterner {
public:
  SymbolInterner();
  ~SymbolInterner();

  /// Intern a spelling. Returns existing id if already present.
  SymbolId intern(std::string_view spelling);

  /// Look up an existing symbol without creating. Returns kInvalidSymbolId if not found.
  SymbolId lookup(std::string_view spelling) const;

  /// Resolve an id back to its spelling. Precondition: id is valid.
  std::string_view resolve(SymbolId id) const;

  /// Check if a spelling is already interned.
  bool contains(std::string_view spelling) const;

  /// Number of interned symbols.
  std::size_t size() const noexcept { return symbols_.empty() ? 0 : symbols_.size() - 1; }

  /// Pre-allocate storage for expected number of unique names.
  void reserve(std::size_t expected);

private:
  // deque guarantees pointer stability on push_back.
  // string_view keys in map_ point into these strings.
  std::deque<std::string> symbols_;

  struct TransparentHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view sv) const noexcept {
      return std::hash<std::string_view>{}(sv);
    }
    std::size_t operator()(const std::string& s) const noexcept {
      return std::hash<std::string>{}(s);
    }
  };

  struct TransparentEq {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept {
      return a == b;
    }
  };

  // Map from string_view (into symbols_ deque) to SymbolId (1-based, stored as id-1 index)
  std::unordered_map<std::string_view, SymbolId, TransparentHash, TransparentEq> map_;
};

} // namespace styio::session

#endif // STYIO_SESSION_SYMBOL_INTERNER_HPP_
