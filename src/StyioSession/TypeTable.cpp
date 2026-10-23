#include "TypeTable.hpp"

namespace styio::session {
namespace {

SymbolId
intern_type_name(SymbolInterner* symbols, std::string_view spelling) {
  if (symbols == nullptr || spelling.empty()) {
    return kInvalidSymbolId;
  }
  // SymbolInterner supports transparent string_view lookup; avoid creating a
  // temporary std::string for every canonical type-key construction.
  return symbols->intern(spelling);
}

TypeKey
make_builtin_key(
  StyioDataTypeOption option,
  std::string_view name,
  std::uint16_t bit_width,
  SymbolInterner* symbols
) {
  TypeKey key;
  key.option = option;
  key.name_id = intern_type_name(symbols, name);
  key.bit_width = bit_width;
  return key;
}

} // namespace

TypeTable::TypeTable() {
  // Reserve id 0 as invalid sentinel
  types_.push_back(TypeKey{});
}

TypeId TypeTable::intern(const TypeKey& key) {
  ++intern_calls_;
  // Canonical builtins are requested repeatedly during semantic analysis.
  // Resolve them before hashing the full structural key; this preserves the
  // TypeId contract while avoiding repeated unordered_map work.
  const TypeId builtin_ids[] = {
    builtin_i64_, builtin_f64_, builtin_bool_, builtin_string_, builtin_void_};
  for (TypeId builtin_id : builtin_ids) {
    if (builtin_id != kInvalidTypeId
        && builtin_id < types_.size()
        && types_[builtin_id] == key) {
      ++builtin_cache_hits_;
      return builtin_id;
    }
  }
  ++lookup_probes_;
  auto it = map_.find(key);
  if (it != map_.end()) {
    return it->second;
  }
  TypeId id = static_cast<TypeId>(types_.size());
  types_.push_back(key);
  map_.emplace(types_.back(), id);
  ++insertions_;
  // Lazily pin canonical scalar IDs as they first appear.  CompilationSession
  // intentionally does not pre-register builtins, so this keeps the same
  // first-use TypeId ordering while enabling the direct path on repeats.
  if (builtin_i64_ == kInvalidTypeId
      && key.option == StyioDataTypeOption::Integer
      && key.bit_width == 64) {
    builtin_i64_ = id;
  }
  if (builtin_f64_ == kInvalidTypeId
      && key.option == StyioDataTypeOption::Float
      && key.bit_width == 64) {
    builtin_f64_ = id;
  }
  if (builtin_bool_ == kInvalidTypeId
      && key.option == StyioDataTypeOption::Bool
      && key.bit_width == 1) {
    builtin_bool_ = id;
  }
  if (builtin_string_ == kInvalidTypeId
      && key.option == StyioDataTypeOption::String
      && key.bit_width == 0) {
    builtin_string_ = id;
  }
  if (builtin_void_ == kInvalidTypeId
      && key.option == StyioDataTypeOption::Undefined
      && key.bit_width == 0) {
    builtin_void_ = id;
  }
  return id;
}

TypeId TypeTable::intern(const StyioDataType& type, SymbolInterner& symbols) {
  return intern(make_key(type, symbols));
}

const TypeKey& TypeTable::resolve(TypeId id) const {
  if (id == kInvalidTypeId || id >= types_.size()) {
    return types_[0]; // invalid sentinel
  }
  return types_[id];
}

TypeKey TypeTable::make_key(const StyioDataType& type, SymbolInterner& symbols) {
  const auto view = styio_canonical_type_view(type);
  TypeKey key;
  key.option = view.option;
  key.name_id = intern_type_name(&symbols, view.name);
  key.bit_width = static_cast<std::uint16_t>(view.num_of_bit);
  key.handle_family = view.handle_family;
  key.state = view.state;
  key.capabilities = view.capabilities;
  key.item_type_name_id = intern_type_name(&symbols, view.item_type_name);
  key.key_type_name_id = intern_type_name(&symbols, view.key_type_name);
  key.has_std_stream_kind = view.has_std_stream_kind;
  key.std_stream_kind = view.std_stream_kind;
  key.resource_value_type_name_id = intern_type_name(&symbols, view.resource_value_type_name);
  key.is_resource_type = view.is_resource_type;
  key.value_family = view.value_family;
  key.item_value_family = view.item_value_family;
  key.key_value_family = view.key_value_family;
  key.resource_shape = view.resource_shape;
  key.resource_shape_bound = view.resource_shape_bound;
  return key;
}

void TypeTable::register_builtins(SymbolInterner* symbols) {
  // Pre-register built-in types.
  builtin_void_ = intern(make_builtin_key(
    StyioDataTypeOption::Undefined,
    "undefined",
    0,
    symbols));

  builtin_bool_ = intern(make_builtin_key(
    StyioDataTypeOption::Bool,
    "bool",
    1,
    symbols));

  builtin_i64_ = intern(make_builtin_key(
    StyioDataTypeOption::Integer,
    "i64",
    64,
    symbols));

  builtin_f64_ = intern(make_builtin_key(
    StyioDataTypeOption::Float,
    "f64",
    64,
    symbols));

  builtin_string_ = intern(make_builtin_key(
    StyioDataTypeOption::String,
    "string",
    0,
    symbols));
}

} // namespace styio::session
