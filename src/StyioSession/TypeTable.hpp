#pragma once
#ifndef STYIO_SESSION_TYPE_TABLE_HPP_
#define STYIO_SESSION_TYPE_TABLE_HPP_

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../StyioToken/Token.hpp"
#include "SymbolInterner.hpp"

namespace styio::session {

using TypeId = std::uint32_t;
inline constexpr TypeId kInvalidTypeId = 0;

/// Packed type descriptor using SymbolIds instead of strings.
/// Enables O(1) hashing and equality comparison.
struct TypeKey {
  StyioDataTypeOption option = StyioDataTypeOption::Undefined;
  SymbolId name_id = kInvalidSymbolId;
  std::uint16_t bit_width = 0;
  StyioHandleFamily handle_family = StyioHandleFamily::None;
  StyioTypeState state = StyioTypeState::None;
  std::uint32_t capabilities = 0;
  SymbolId item_type_name_id = kInvalidSymbolId;
  SymbolId key_type_name_id = kInvalidSymbolId;
  SymbolId resource_value_type_name_id = kInvalidSymbolId;
  bool is_resource_type = false;
  StyioValueFamily value_family = StyioValueFamily::Unknown;
  StyioValueFamily item_value_family = StyioValueFamily::Unknown;
  StyioValueFamily key_value_family = StyioValueFamily::Unknown;
  StyioResourceShapeKind resource_shape = StyioResourceShapeKind::None;
  std::size_t resource_shape_bound = 0;

  bool operator==(const TypeKey&) const = default; // C++20
};

struct TypeKeyHash {
  std::size_t operator()(const TypeKey& k) const noexcept {
    // Simple hash combining first few fields (most discriminative)
    std::size_t h = static_cast<std::size_t>(k.option);
    h = h * 31 + static_cast<std::size_t>(k.name_id);
    h = h * 31 + static_cast<std::size_t>(k.bit_width);
    h = h * 31 + static_cast<std::size_t>(k.handle_family);
    h = h * 31 + static_cast<std::size_t>(k.item_type_name_id);
    h = h * 31 + static_cast<std::size_t>(k.key_type_name_id);
    h = h * 31 + static_cast<std::size_t>(k.capabilities);
    return h;
  }
};

/// Session-local canonical type table.
/// Each distinct type gets a stable TypeId; equality is O(1).
class TypeTable {
public:
  TypeTable();

  /// Intern a type — returns existing id if already canonicalized.
  TypeId intern(const TypeKey& key);

  /// Resolve a TypeId back to its TypeKey.
  const TypeKey& resolve(TypeId id) const;

  /// O(1) type equality.
  bool equals(TypeId a, TypeId b) const noexcept { return a == b; }

  /// Pre-register built-in types. Call once after interner is populated.
  void register_builtins();

  TypeId builtin_i64() const noexcept { return builtin_i64_; }
  TypeId builtin_f64() const noexcept { return builtin_f64_; }
  TypeId builtin_bool() const noexcept { return builtin_bool_; }
  TypeId builtin_string() const noexcept { return builtin_string_; }
  TypeId builtin_void() const noexcept { return builtin_void_; }

  std::size_t size() const noexcept { return types_.size() - 1; }
  void reserve(std::size_t n) { types_.reserve(n + 1); map_.reserve(n); }

private:
  std::vector<TypeKey> types_;  // id 0 = invalid sentinel
  std::unordered_map<TypeKey, TypeId, TypeKeyHash> map_;

  TypeId builtin_i64_ = kInvalidTypeId;
  TypeId builtin_f64_ = kInvalidTypeId;
  TypeId builtin_bool_ = kInvalidTypeId;
  TypeId builtin_string_ = kInvalidTypeId;
  TypeId builtin_void_ = kInvalidTypeId;
};

} // namespace styio::session

#endif // STYIO_SESSION_TYPE_TABLE_HPP_
