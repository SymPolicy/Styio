#include "TypeTable.hpp"

namespace styio::session {

TypeTable::TypeTable() {
  // Reserve id 0 as invalid sentinel
  types_.push_back(TypeKey{});
}

TypeId TypeTable::intern(const TypeKey& key) {
  auto it = map_.find(key);
  if (it != map_.end()) {
    return it->second;
  }
  TypeId id = static_cast<TypeId>(types_.size());
  types_.push_back(key);
  map_.emplace(types_.back(), id);
  return id;
}

const TypeKey& TypeTable::resolve(TypeId id) const {
  if (id == kInvalidTypeId || id >= types_.size()) {
    return types_[0]; // invalid sentinel
  }
  return types_[id];
}

void TypeTable::register_builtins() {
  // Pre-register built-in types.
  builtin_void_ = intern(TypeKey{
    StyioDataTypeOption::Undefined, kInvalidSymbolId, 0,
    StyioHandleFamily::None, StyioTypeState::None, 0
  });

  builtin_bool_ = intern(TypeKey{
    StyioDataTypeOption::Bool, kInvalidSymbolId, 1,
    StyioHandleFamily::None, StyioTypeState::None, 0
  });

  builtin_i64_ = intern(TypeKey{
    StyioDataTypeOption::Integer, kInvalidSymbolId, 64,
    StyioHandleFamily::None, StyioTypeState::None, 0
  });

  builtin_f64_ = intern(TypeKey{
    StyioDataTypeOption::Float, kInvalidSymbolId, 64,
    StyioHandleFamily::None, StyioTypeState::None, 0
  });

  builtin_string_ = intern(TypeKey{
    StyioDataTypeOption::String, kInvalidSymbolId, 0,
    StyioHandleFamily::None, StyioTypeState::None, 0
  });
}

} // namespace styio::session
