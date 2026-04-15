#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <cerrno>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ExternLib.hpp"
#include "StyioConfig/NanoProfile.hpp"
#include "StyioRuntime/HandleTable.hpp"

namespace {

/* Alternating buffers so two consecutive reads (e.g. zip of two files) keep both lines valid. */
thread_local char g_read_line_bufs[2][65536];
thread_local int g_read_line_buf_which = 0;
thread_local StyioHandleTable g_handle_table;
thread_local std::unordered_set<const void*> g_owned_cstr_ptrs;
thread_local bool g_runtime_error = false;
thread_local std::string g_runtime_error_message;
thread_local std::string g_runtime_error_subcode;

constexpr const char* kRuntimeSubcodeInvalidFileHandle = "STYIO_RUNTIME_INVALID_FILE_HANDLE";
constexpr const char* kRuntimeSubcodeFilePathNull = "STYIO_RUNTIME_FILE_PATH_NULL";
constexpr const char* kRuntimeSubcodeFileOpenRead = "STYIO_RUNTIME_FILE_OPEN_READ";
constexpr const char* kRuntimeSubcodeFileOpenWrite = "STYIO_RUNTIME_FILE_OPEN_WRITE";
constexpr const char* kRuntimeSubcodeInvalidListHandle = "STYIO_RUNTIME_INVALID_LIST_HANDLE";
constexpr const char* kRuntimeSubcodeInvalidDictHandle = "STYIO_RUNTIME_INVALID_DICT_HANDLE";
constexpr const char* kRuntimeSubcodeListParse = "STYIO_RUNTIME_LIST_PARSE";
constexpr const char* kRuntimeSubcodeListIndex = "STYIO_RUNTIME_LIST_INDEX";
constexpr const char* kRuntimeSubcodeListElemKind = "STYIO_RUNTIME_LIST_ELEM_KIND";
constexpr const char* kRuntimeSubcodeDictKey = "STYIO_RUNTIME_DICT_KEY";
constexpr const char* kRuntimeSubcodeNumericParse = "STYIO_RUNTIME_NUMERIC_PARSE";

enum class StyioListElemKind : std::uint8_t
{
  Bool = 0,
  I64 = 1,
  F64 = 2,
  String = 3,
  ListHandle = 4,
  DictHandle = 5,
};

struct StyioListBase
{
  explicit StyioListBase(StyioListElemKind kind) :
      elem_kind(kind) {
  }

  virtual ~StyioListBase() = default;

  StyioListElemKind elem_kind;
};

struct StyioListI64 : public StyioListBase
{
  StyioListI64() :
      StyioListBase(StyioListElemKind::I64) {
  }

  std::vector<int64_t> elems;
};

struct StyioListBool : public StyioListBase
{
  StyioListBool() :
      StyioListBase(StyioListElemKind::Bool) {
  }

  std::vector<int64_t> elems;
};

struct StyioListF64 : public StyioListBase
{
  StyioListF64() :
      StyioListBase(StyioListElemKind::F64) {
  }

  std::vector<double> elems;
};

struct StyioListString : public StyioListBase
{
  StyioListString() :
      StyioListBase(StyioListElemKind::String) {
  }

  std::vector<std::string> elems;
};

struct StyioListListHandle : public StyioListBase
{
  StyioListListHandle() :
      StyioListBase(StyioListElemKind::ListHandle) {
  }

  std::vector<int64_t> elems;
};

struct StyioListDictHandle : public StyioListBase
{
  StyioListDictHandle() :
      StyioListBase(StyioListElemKind::DictHandle) {
  }

  std::vector<int64_t> elems;
};

enum class StyioDictValueKind : std::uint8_t
{
  Bool = 0,
  I64 = 1,
  F64 = 2,
  String = 3,
  ListHandle = 4,
  DictHandle = 5,
};

enum class StyioDictRuntimeImpl : std::uint8_t
{
  OrderedHash = 0,
  Linear = 1,
};

struct StyioDictBackendSpec
{
  StyioDictRuntimeImpl impl;
  const char* name;
  const char* aliases[3];
};

constexpr StyioDictBackendSpec kStyioDictBackendRegistry[] = {
#if STYIO_NANO_ENABLE_DICT_BACKEND_ORDERED_HASH
  {StyioDictRuntimeImpl::OrderedHash, "ordered-hash", {"ordered_hash", "v2", nullptr}},
#endif
#if STYIO_NANO_ENABLE_DICT_BACKEND_LINEAR
  {StyioDictRuntimeImpl::Linear, "linear", {"v1", nullptr, nullptr}},
#endif
};

constexpr int kStyioDictBackendRegistryCount =
  static_cast<int>(sizeof(kStyioDictBackendRegistry) / sizeof(kStyioDictBackendRegistry[0]));
static_assert(kStyioDictBackendRegistryCount > 0, "styio dict registry must expose at least one backend");

struct StyioDictBase
{
  explicit StyioDictBase(StyioDictValueKind kind, StyioDictRuntimeImpl impl) :
      value_kind(kind),
      runtime_impl(impl) {
  }

  virtual ~StyioDictBase() = default;

  StyioDictValueKind value_kind;
  StyioDictRuntimeImpl runtime_impl;
};

template <typename T, StyioDictValueKind Kind>
struct StyioDictStorage : public StyioDictBase
{
  using mapped_type = T;

  explicit StyioDictStorage(StyioDictRuntimeImpl impl = StyioDictRuntimeImpl::OrderedHash) :
      StyioDictBase(Kind, impl) {
  }

  std::vector<std::pair<std::string, T>> entries;
  std::unordered_map<std::string, size_t> index_by_key;
};

using StyioDictBool = StyioDictStorage<int64_t, StyioDictValueKind::Bool>;
using StyioDictI64 = StyioDictStorage<int64_t, StyioDictValueKind::I64>;
using StyioDictF64 = StyioDictStorage<double, StyioDictValueKind::F64>;
using StyioDictString = StyioDictStorage<std::string, StyioDictValueKind::String>;
using StyioDictListHandle = StyioDictStorage<int64_t, StyioDictValueKind::ListHandle>;
using StyioDictDictHandle = StyioDictStorage<int64_t, StyioDictValueKind::DictHandle>;

thread_local int64_t g_active_list_handles = 0;
thread_local int64_t g_active_dict_handles = 0;
thread_local StyioDictRuntimeImpl g_default_dict_runtime_impl = StyioDictRuntimeImpl::OrderedHash;

void close_list(void* raw);
void close_dict(void* raw);
int64_t clone_list_handle_value(int64_t h);
int64_t clone_dict_handle_value(int64_t h);
void append_list_handle_repr(std::string& out, int64_t h);
void append_dict_handle_repr(std::string& out, int64_t h);

thread_local struct HandleTableCleanup {
  ~HandleTableCleanup() {
    g_handle_table.release_all(
      StyioHandleTable::HandleKind::File,
      [](void* raw) { std::fclose(static_cast<FILE*>(raw)); });
    g_handle_table.release_all(
      StyioHandleTable::HandleKind::List,
      [](void* raw) { close_list(raw); });
    g_handle_table.release_all(
      StyioHandleTable::HandleKind::Dict,
      [](void* raw) { close_dict(raw); });
  }
} g_handle_table_cleanup;

std::string
lower_ascii_copy(const char* raw) {
  if (raw == nullptr) {
    return {};
  }
  std::string lowered;
  lowered.reserve(std::strlen(raw));
  for (const char* p = raw; *p != '\0'; ++p) {
    lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
  }
  return lowered;
}

const StyioDictBackendSpec*
dict_backend_spec_by_impl(StyioDictRuntimeImpl impl) {
  for (const auto& spec : kStyioDictBackendRegistry) {
    if (spec.impl == impl) {
      return &spec;
    }
  }
  return nullptr;
}

const StyioDictBackendSpec*
dict_backend_spec_by_name(const char* raw_name) {
  const std::string needle = lower_ascii_copy(raw_name);
  if (needle.empty()) {
    return nullptr;
  }
  for (const auto& spec : kStyioDictBackendRegistry) {
    if (needle == spec.name) {
      return &spec;
    }
    for (const char* alias : spec.aliases) {
      if (alias != nullptr && needle == alias) {
        return &spec;
      }
    }
  }
  return nullptr;
}

const StyioDictBackendSpec*
current_dict_backend_spec() {
  const auto* spec = dict_backend_spec_by_impl(g_default_dict_runtime_impl);
  if (spec != nullptr) {
    return spec;
  }
  return &kStyioDictBackendRegistry[0];
}

std::string
resolve_read_path(const char* path) {
  if (path == nullptr) {
    return {};
  }

  std::string p(path);
  std::error_code ec;
  if (std::filesystem::exists(p, ec)) {
    return p;
  }

  constexpr const char* kTestsPrefix = "tests/";
  constexpr const char* kMilestonesPrefix = "tests/milestones/";
  if (p.rfind(kTestsPrefix, 0) != 0 || p.rfind(kMilestonesPrefix, 0) == 0) {
    return p;
  }

  std::string tail = p.substr(std::strlen(kTestsPrefix)); // m5/data/...
  const size_t slash = tail.find('/');
  if (slash == std::string::npos) {
    return p;
  }

  const std::string top = tail.substr(0, slash); // m5
  if (top.size() < 2 || top[0] != 'm') {
    return p;
  }
  for (size_t i = 1; i < top.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(top[i]))) {
      return p;
    }
  }

  std::string candidate = std::string(kMilestonesPrefix) + tail;
  if (std::filesystem::exists(candidate, ec)) {
    return candidate;
  }
  return p;
}

void
set_runtime_error_once(const char* subcode, const std::string& message) {
  g_runtime_error = true;
  if (g_runtime_error_message.empty()) {
    g_runtime_error_message = message;
    g_runtime_error_subcode = (subcode != nullptr) ? subcode : "";
  }
}

int64_t
stash_file(FILE* f) {
  return g_handle_table.acquire(StyioHandleTable::HandleKind::File, f);
}

FILE*
as_file(int64_t h, bool diagnose_if_missing = false) {
  if (h == 0) {
    return nullptr;
  }
  FILE* f = g_handle_table.lookup_as<FILE>(h, StyioHandleTable::HandleKind::File);
  if (f == nullptr && diagnose_if_missing) {
    set_runtime_error_once(
      kRuntimeSubcodeInvalidFileHandle,
      "invalid file handle: " + std::to_string(static_cast<long long>(h)));
  }
  return f;
}

void
close_file(void* raw) {
  if (raw == nullptr) {
    return;
  }
  std::fclose(static_cast<FILE*>(raw));
}

int64_t
stash_list(StyioListBase* list) {
  if (list == nullptr) {
    return 0;
  }
  ++g_active_list_handles;
  return g_handle_table.acquire(StyioHandleTable::HandleKind::List, list);
}

StyioListBase*
as_list_base(int64_t h, bool diagnose_if_missing = false) {
  if (h == 0) {
    return nullptr;
  }
  auto* list = g_handle_table.lookup_as<StyioListBase>(h, StyioHandleTable::HandleKind::List);
  if (list == nullptr && diagnose_if_missing) {
    set_runtime_error_once(
      kRuntimeSubcodeInvalidListHandle,
      "invalid list handle: " + std::to_string(static_cast<long long>(h)));
  }
  return list;
}

StyioListI64*
as_list_i64(int64_t h, bool diagnose_if_missing = false) {
  StyioListBase* list = as_list_base(h, diagnose_if_missing);
  if (list == nullptr) {
    return nullptr;
  }
  if (list->elem_kind != StyioListElemKind::I64) {
    if (diagnose_if_missing) {
      set_runtime_error_once(
        kRuntimeSubcodeListElemKind,
        "list handle does not carry i64 elements: " + std::to_string(static_cast<long long>(h)));
    }
    return nullptr;
  }
  return static_cast<StyioListI64*>(list);
}

StyioListBool*
as_list_bool(int64_t h, bool diagnose_if_missing = false) {
  StyioListBase* list = as_list_base(h, diagnose_if_missing);
  if (list == nullptr) {
    return nullptr;
  }
  if (list->elem_kind != StyioListElemKind::Bool) {
    if (diagnose_if_missing) {
      set_runtime_error_once(
        kRuntimeSubcodeListElemKind,
        "list handle does not carry bool elements: " + std::to_string(static_cast<long long>(h)));
    }
    return nullptr;
  }
  return static_cast<StyioListBool*>(list);
}

StyioListF64*
as_list_f64(int64_t h, bool diagnose_if_missing = false) {
  StyioListBase* list = as_list_base(h, diagnose_if_missing);
  if (list == nullptr) {
    return nullptr;
  }
  if (list->elem_kind != StyioListElemKind::F64) {
    if (diagnose_if_missing) {
      set_runtime_error_once(
        kRuntimeSubcodeListElemKind,
        "list handle does not carry f64 elements: " + std::to_string(static_cast<long long>(h)));
    }
    return nullptr;
  }
  return static_cast<StyioListF64*>(list);
}

StyioListString*
as_list_string(int64_t h, bool diagnose_if_missing = false) {
  StyioListBase* list = as_list_base(h, diagnose_if_missing);
  if (list == nullptr) {
    return nullptr;
  }
  if (list->elem_kind != StyioListElemKind::String) {
    if (diagnose_if_missing) {
      set_runtime_error_once(
        kRuntimeSubcodeListElemKind,
        "list handle does not carry string elements: " + std::to_string(static_cast<long long>(h)));
    }
    return nullptr;
  }
  return static_cast<StyioListString*>(list);
}

StyioListListHandle*
as_list_list_handle(int64_t h, bool diagnose_if_missing = false) {
  StyioListBase* list = as_list_base(h, diagnose_if_missing);
  if (list == nullptr) {
    return nullptr;
  }
  if (list->elem_kind != StyioListElemKind::ListHandle) {
    if (diagnose_if_missing) {
      set_runtime_error_once(
        kRuntimeSubcodeListElemKind,
        "list handle does not carry list-handle elements: "
          + std::to_string(static_cast<long long>(h)));
    }
    return nullptr;
  }
  return static_cast<StyioListListHandle*>(list);
}

StyioListDictHandle*
as_list_dict_handle(int64_t h, bool diagnose_if_missing = false) {
  StyioListBase* list = as_list_base(h, diagnose_if_missing);
  if (list == nullptr) {
    return nullptr;
  }
  if (list->elem_kind != StyioListElemKind::DictHandle) {
    if (diagnose_if_missing) {
      set_runtime_error_once(
        kRuntimeSubcodeListElemKind,
        "list handle does not carry dict-handle elements: "
          + std::to_string(static_cast<long long>(h)));
    }
    return nullptr;
  }
  return static_cast<StyioListDictHandle*>(list);
}

void
close_list(void* raw) {
  if (raw == nullptr) {
    return;
  }
  auto* list = static_cast<StyioListBase*>(raw);
  switch (list->elem_kind) {
    case StyioListElemKind::Bool:
      delete static_cast<StyioListBool*>(list);
      break;
    case StyioListElemKind::I64:
      delete static_cast<StyioListI64*>(list);
      break;
    case StyioListElemKind::F64:
      delete static_cast<StyioListF64*>(list);
      break;
    case StyioListElemKind::String:
      delete static_cast<StyioListString*>(list);
      break;
    case StyioListElemKind::ListHandle: {
      auto* handles = static_cast<StyioListListHandle*>(list);
      for (int64_t elem : handles->elems) {
        (void)g_handle_table.release(elem, StyioHandleTable::HandleKind::List, close_list);
      }
      delete handles;
    } break;
    case StyioListElemKind::DictHandle: {
      auto* handles = static_cast<StyioListDictHandle*>(list);
      for (int64_t elem : handles->elems) {
        (void)g_handle_table.release(elem, StyioHandleTable::HandleKind::Dict, close_dict);
      }
      delete handles;
    } break;
  }
  if (g_active_list_handles > 0) {
    --g_active_list_handles;
  }
}

int64_t
stash_dict(StyioDictBase* dict) {
  if (dict == nullptr) {
    return 0;
  }
  ++g_active_dict_handles;
  return g_handle_table.acquire(StyioHandleTable::HandleKind::Dict, dict);
}

StyioDictBase*
as_dict_base(int64_t h, bool diagnose_if_missing = false) {
  if (h == 0) {
    return nullptr;
  }
  auto* dict = g_handle_table.lookup_as<StyioDictBase>(h, StyioHandleTable::HandleKind::Dict);
  if (dict == nullptr && diagnose_if_missing) {
    set_runtime_error_once(
      kRuntimeSubcodeInvalidDictHandle,
      "invalid dict handle: " + std::to_string(static_cast<long long>(h)));
  }
  return dict;
}

StyioDictBool*
as_dict_bool(int64_t h, bool diagnose_if_missing = false) {
  StyioDictBase* dict = as_dict_base(h, diagnose_if_missing);
  if (dict == nullptr) {
    return nullptr;
  }
  if (dict->value_kind != StyioDictValueKind::Bool) {
    if (diagnose_if_missing) {
      set_runtime_error_once(kRuntimeSubcodeInvalidDictHandle, "dict handle does not carry bool values");
    }
    return nullptr;
  }
  return static_cast<StyioDictBool*>(dict);
}

StyioDictI64*
as_dict_i64(int64_t h, bool diagnose_if_missing = false) {
  StyioDictBase* dict = as_dict_base(h, diagnose_if_missing);
  if (dict == nullptr) {
    return nullptr;
  }
  if (dict->value_kind != StyioDictValueKind::I64) {
    if (diagnose_if_missing) {
      set_runtime_error_once(kRuntimeSubcodeInvalidDictHandle, "dict handle does not carry i64 values");
    }
    return nullptr;
  }
  return static_cast<StyioDictI64*>(dict);
}

StyioDictF64*
as_dict_f64(int64_t h, bool diagnose_if_missing = false) {
  StyioDictBase* dict = as_dict_base(h, diagnose_if_missing);
  if (dict == nullptr) {
    return nullptr;
  }
  if (dict->value_kind != StyioDictValueKind::F64) {
    if (diagnose_if_missing) {
      set_runtime_error_once(kRuntimeSubcodeInvalidDictHandle, "dict handle does not carry f64 values");
    }
    return nullptr;
  }
  return static_cast<StyioDictF64*>(dict);
}

StyioDictString*
as_dict_string(int64_t h, bool diagnose_if_missing = false) {
  StyioDictBase* dict = as_dict_base(h, diagnose_if_missing);
  if (dict == nullptr) {
    return nullptr;
  }
  if (dict->value_kind != StyioDictValueKind::String) {
    if (diagnose_if_missing) {
      set_runtime_error_once(kRuntimeSubcodeInvalidDictHandle, "dict handle does not carry string values");
    }
    return nullptr;
  }
  return static_cast<StyioDictString*>(dict);
}

StyioDictListHandle*
as_dict_list(int64_t h, bool diagnose_if_missing = false) {
  StyioDictBase* dict = as_dict_base(h, diagnose_if_missing);
  if (dict == nullptr) {
    return nullptr;
  }
  if (dict->value_kind != StyioDictValueKind::ListHandle) {
    if (diagnose_if_missing) {
      set_runtime_error_once(kRuntimeSubcodeInvalidDictHandle, "dict handle does not carry list values");
    }
    return nullptr;
  }
  return static_cast<StyioDictListHandle*>(dict);
}

StyioDictDictHandle*
as_dict_dict(int64_t h, bool diagnose_if_missing = false) {
  StyioDictBase* dict = as_dict_base(h, diagnose_if_missing);
  if (dict == nullptr) {
    return nullptr;
  }
  if (dict->value_kind != StyioDictValueKind::DictHandle) {
    if (diagnose_if_missing) {
      set_runtime_error_once(kRuntimeSubcodeInvalidDictHandle, "dict handle does not carry dict values");
    }
    return nullptr;
  }
  return static_cast<StyioDictDictHandle*>(dict);
}

template <typename DictT>
void
rebuild_dict_index(DictT* dict) {
  if (dict == nullptr) {
    return;
  }
  dict->index_by_key.clear();
  dict->index_by_key.reserve(dict->entries.size());
  for (size_t i = 0; i < dict->entries.size(); ++i) {
    dict->index_by_key[dict->entries[i].first] = i;
  }
}

template <typename DictT>
bool
dict_find_pos(const DictT* dict, const char* key, size_t& pos) {
  if (dict == nullptr || key == nullptr) {
    return false;
  }
  const auto* backend = dict_backend_spec_by_impl(dict->runtime_impl);
  if (backend == nullptr) {
    return false;
  }
  switch (backend->impl) {
    case StyioDictRuntimeImpl::OrderedHash: {
      auto it = dict->index_by_key.find(key);
      if (it == dict->index_by_key.end()) {
        return false;
      }
      pos = it->second;
      return pos < dict->entries.size();
    }
    case StyioDictRuntimeImpl::Linear:
      for (size_t i = 0; i < dict->entries.size(); ++i) {
        if (dict->entries[i].first == key) {
          pos = i;
          return true;
        }
      }
      return false;
  }
  return false;
}

template <typename DictT>
void
dict_after_clone(DictT* dict) {
  if (dict == nullptr) {
    return;
  }
  const auto* backend = dict_backend_spec_by_impl(dict->runtime_impl);
  if (backend == nullptr) {
    dict->index_by_key.clear();
    return;
  }
  switch (backend->impl) {
    case StyioDictRuntimeImpl::OrderedHash:
      rebuild_dict_index(dict);
      return;
    case StyioDictRuntimeImpl::Linear:
      dict->index_by_key.clear();
      return;
  }
}

template <typename DictT, typename ValueT>
void
dict_set(DictT* dict, const char* key, ValueT&& value) {
  if (dict == nullptr || key == nullptr) {
    return;
  }
  const auto* backend = dict_backend_spec_by_impl(dict->runtime_impl);
  if (backend == nullptr) {
    return;
  }
  switch (backend->impl) {
    case StyioDictRuntimeImpl::OrderedHash: {
      auto it = dict->index_by_key.find(key);
      if (it != dict->index_by_key.end() && it->second < dict->entries.size()) {
        dict->entries[it->second].second = std::forward<ValueT>(value);
        return;
      }
      dict->entries.emplace_back(std::string(key), std::forward<ValueT>(value));
      dict->index_by_key[dict->entries.back().first] = dict->entries.size() - 1;
      return;
    }
    case StyioDictRuntimeImpl::Linear:
      for (auto& entry : dict->entries) {
        if (entry.first == key) {
          entry.second = std::forward<ValueT>(value);
          return;
        }
      }
      dict->entries.emplace_back(std::string(key), std::forward<ValueT>(value));
      return;
  }
}

template <typename DictT, typename ReleaseFn>
void
dict_set_handle(DictT* dict, const char* key, int64_t value, ReleaseFn&& release_existing) {
  if (dict == nullptr || key == nullptr) {
    return;
  }
  const auto* backend = dict_backend_spec_by_impl(dict->runtime_impl);
  if (backend == nullptr) {
    return;
  }
  switch (backend->impl) {
    case StyioDictRuntimeImpl::OrderedHash: {
      auto it = dict->index_by_key.find(key);
      if (it != dict->index_by_key.end() && it->second < dict->entries.size()) {
        release_existing(dict->entries[it->second].second);
        dict->entries[it->second].second = value;
        return;
      }
      dict->entries.emplace_back(std::string(key), value);
      dict->index_by_key[dict->entries.back().first] = dict->entries.size() - 1;
      return;
    }
    case StyioDictRuntimeImpl::Linear:
      for (auto& entry : dict->entries) {
        if (entry.first == key) {
          release_existing(entry.second);
          entry.second = value;
          return;
        }
      }
      dict->entries.emplace_back(std::string(key), value);
      return;
  }
}

template <typename DictT>
DictT*
make_dict_storage_for_current_backend() {
  return new DictT(current_dict_backend_spec()->impl);
}

template <typename DictT>
DictT*
clone_dict_storage(const DictT* src, StyioDictRuntimeImpl impl) {
  auto* clone = new DictT(impl);
  clone->entries = src->entries;
  dict_after_clone(clone);
  return clone;
}

template <typename DictT>
bool
dict_try_get(const DictT* dict, const char* key, typename DictT::mapped_type& out) {
  size_t pos = 0;
  if (!dict_find_pos(dict, key, pos)) {
    return false;
  }
  out = dict->entries[pos].second;
  return true;
}

void
close_dict(void* raw) {
  if (raw == nullptr) {
    return;
  }
  auto* dict = static_cast<StyioDictBase*>(raw);
  switch (dict->value_kind) {
    case StyioDictValueKind::Bool:
      delete static_cast<StyioDictBool*>(dict);
      break;
    case StyioDictValueKind::I64:
      delete static_cast<StyioDictI64*>(dict);
      break;
    case StyioDictValueKind::F64:
      delete static_cast<StyioDictF64*>(dict);
      break;
    case StyioDictValueKind::String:
      delete static_cast<StyioDictString*>(dict);
      break;
    case StyioDictValueKind::ListHandle: {
      auto* values = static_cast<StyioDictListHandle*>(dict);
      for (const auto& entry : values->entries) {
        (void)g_handle_table.release(entry.second, StyioHandleTable::HandleKind::List, close_list);
      }
      delete values;
    } break;
    case StyioDictValueKind::DictHandle: {
      auto* values = static_cast<StyioDictDictHandle*>(dict);
      for (const auto& entry : values->entries) {
        (void)g_handle_table.release(entry.second, StyioHandleTable::HandleKind::Dict, close_dict);
      }
      delete values;
    } break;
  }
  if (g_active_dict_handles > 0) {
    --g_active_dict_handles;
  }
}

void
skip_ws(const std::string& input, size_t& pos) {
  while (pos < input.size()
         && std::isspace(static_cast<unsigned char>(input[pos]))) {
    ++pos;
  }
}

bool
parse_i64_list_literal(const std::string& input, std::vector<int64_t>& out) {
  size_t pos = 0;
  skip_ws(input, pos);
  if (pos >= input.size() || input[pos] != '[') {
    return false;
  }
  ++pos;
  skip_ws(input, pos);
  if (pos < input.size() && input[pos] == ']') {
    ++pos;
    skip_ws(input, pos);
    return pos == input.size();
  }

  while (pos < input.size()) {
    const char* begin = input.c_str() + pos;
    char* end = nullptr;
    long long v = std::strtoll(begin, &end, 10);
    if (end == begin) {
      return false;
    }
    out.push_back(static_cast<int64_t>(v));
    pos = static_cast<size_t>(end - input.c_str());
    skip_ws(input, pos);
    if (pos >= input.size()) {
      return false;
    }
    if (input[pos] == ',') {
      ++pos;
      skip_ws(input, pos);
      continue;
    }
    if (input[pos] == ']') {
      ++pos;
      skip_ws(input, pos);
      return pos == input.size();
    }
    return false;
  }

  return false;
}

std::string
read_all_stdin() {
  std::string input;
  char buf[4096];
  while (true) {
    size_t n = std::fread(buf, 1, sizeof(buf), stdin);
    if (n == 0) {
      break;
    }
    input.append(buf, n);
  }
  return input;
}

std::string
escape_string_for_literal(const std::string& input) {
  std::string out;
  out.reserve(input.size() + 4);
  for (char ch : input) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  return out;
}

std::string
format_f64_literal(double value) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.6f", value);
  return std::string(buf);
}

const char*
copy_to_owned_cstr(const std::string& value) {
  char* p = static_cast<char*>(std::malloc(value.size() + 1));
  if (p == nullptr) {
    return "";
  }
  std::memcpy(p, value.c_str(), value.size() + 1);
  g_owned_cstr_ptrs.insert(p);
  return p;
}

int64_t
clone_list_handle_value(int64_t h) {
  StyioListBase* src = as_list_base(h, true);
  if (src == nullptr) {
    return 0;
  }
  switch (src->elem_kind) {
    case StyioListElemKind::Bool: {
      auto* clone = new StyioListBool();
      clone->elems = static_cast<StyioListBool*>(src)->elems;
      return stash_list(clone);
    }
    case StyioListElemKind::I64: {
      auto* clone = new StyioListI64();
      clone->elems = static_cast<StyioListI64*>(src)->elems;
      return stash_list(clone);
    }
    case StyioListElemKind::F64: {
      auto* clone = new StyioListF64();
      clone->elems = static_cast<StyioListF64*>(src)->elems;
      return stash_list(clone);
    }
    case StyioListElemKind::String: {
      auto* clone = new StyioListString();
      clone->elems = static_cast<StyioListString*>(src)->elems;
      return stash_list(clone);
    }
    case StyioListElemKind::ListHandle: {
      auto* clone = new StyioListListHandle();
      auto* handles = static_cast<StyioListListHandle*>(src);
      clone->elems.reserve(handles->elems.size());
      for (int64_t elem : handles->elems) {
        clone->elems.push_back(clone_list_handle_value(elem));
      }
      return stash_list(clone);
    }
    case StyioListElemKind::DictHandle: {
      auto* clone = new StyioListDictHandle();
      auto* handles = static_cast<StyioListDictHandle*>(src);
      clone->elems.reserve(handles->elems.size());
      for (int64_t elem : handles->elems) {
        clone->elems.push_back(clone_dict_handle_value(elem));
      }
      return stash_list(clone);
    }
  }
  return 0;
}

int64_t
clone_dict_handle_value(int64_t h) {
  StyioDictBase* src = as_dict_base(h, true);
  if (src == nullptr) {
    return 0;
  }
  switch (src->value_kind) {
    case StyioDictValueKind::Bool:
      return stash_dict(clone_dict_storage(static_cast<StyioDictBool*>(src), src->runtime_impl));
    case StyioDictValueKind::I64:
      return stash_dict(clone_dict_storage(static_cast<StyioDictI64*>(src), src->runtime_impl));
    case StyioDictValueKind::F64:
      return stash_dict(clone_dict_storage(static_cast<StyioDictF64*>(src), src->runtime_impl));
    case StyioDictValueKind::String:
      return stash_dict(clone_dict_storage(static_cast<StyioDictString*>(src), src->runtime_impl));
    case StyioDictValueKind::ListHandle: {
      auto* clone = new StyioDictListHandle(src->runtime_impl);
      auto* values = static_cast<StyioDictListHandle*>(src);
      clone->entries.reserve(values->entries.size());
      for (const auto& entry : values->entries) {
        clone->entries.emplace_back(entry.first, clone_list_handle_value(entry.second));
      }
      dict_after_clone(clone);
      return stash_dict(clone);
    }
    case StyioDictValueKind::DictHandle: {
      auto* clone = new StyioDictDictHandle(src->runtime_impl);
      auto* values = static_cast<StyioDictDictHandle*>(src);
      clone->entries.reserve(values->entries.size());
      for (const auto& entry : values->entries) {
        clone->entries.emplace_back(entry.first, clone_dict_handle_value(entry.second));
      }
      dict_after_clone(clone);
      return stash_dict(clone);
    }
  }
  return 0;
}

void
append_list_handle_repr(std::string& out, int64_t h) {
  StyioListBase* list = as_list_base(h, true);
  if (list != nullptr) {
    std::string text = "[";
    switch (list->elem_kind) {
      case StyioListElemKind::Bool: {
        auto* bools = static_cast<StyioListBool*>(list);
        for (size_t i = 0; i < bools->elems.size(); ++i) {
          if (i > 0) {
            text.push_back(',');
          }
          text += bools->elems[i] != 0 ? "true" : "false";
        }
      } break;
      case StyioListElemKind::I64: {
        auto* ints = static_cast<StyioListI64*>(list);
        for (size_t i = 0; i < ints->elems.size(); ++i) {
          if (i > 0) {
            text.push_back(',');
          }
          text += std::to_string(static_cast<long long>(ints->elems[i]));
        }
      } break;
      case StyioListElemKind::F64: {
        auto* floats = static_cast<StyioListF64*>(list);
        for (size_t i = 0; i < floats->elems.size(); ++i) {
          if (i > 0) {
            text.push_back(',');
          }
          text += format_f64_literal(floats->elems[i]);
        }
      } break;
      case StyioListElemKind::String: {
        auto* strings = static_cast<StyioListString*>(list);
        for (size_t i = 0; i < strings->elems.size(); ++i) {
          if (i > 0) {
            text.push_back(',');
          }
          text.push_back('"');
          text += escape_string_for_literal(strings->elems[i]);
          text.push_back('"');
        }
      } break;
      case StyioListElemKind::ListHandle: {
        auto* lists = static_cast<StyioListListHandle*>(list);
        for (size_t i = 0; i < lists->elems.size(); ++i) {
          if (i > 0) {
            text.push_back(',');
          }
          append_list_handle_repr(text, lists->elems[i]);
        }
      } break;
      case StyioListElemKind::DictHandle: {
        auto* dicts = static_cast<StyioListDictHandle*>(list);
        for (size_t i = 0; i < dicts->elems.size(); ++i) {
          if (i > 0) {
            text.push_back(',');
          }
          append_dict_handle_repr(text, dicts->elems[i]);
        }
      } break;
    }
    text.push_back(']');
    out += text;
    return;
  }
  out += "[]";
}

void
append_dict_handle_repr(std::string& out, int64_t h) {
  StyioDictBase* dict = as_dict_base(h, true);
  if (dict == nullptr) {
    out += "{}";
    return;
  }
  std::string text = "{";
  switch (dict->value_kind) {
    case StyioDictValueKind::Bool: {
      auto* values = static_cast<StyioDictBool*>(dict);
      for (size_t i = 0; i < values->entries.size(); ++i) {
        if (i > 0) {
          text.push_back(',');
        }
        text.push_back('"');
        text += escape_string_for_literal(values->entries[i].first);
        text += "\":";
        text += values->entries[i].second != 0 ? "true" : "false";
      }
    } break;
    case StyioDictValueKind::I64: {
      auto* values = static_cast<StyioDictI64*>(dict);
      for (size_t i = 0; i < values->entries.size(); ++i) {
        if (i > 0) {
          text.push_back(',');
        }
        text.push_back('"');
        text += escape_string_for_literal(values->entries[i].first);
        text += "\":";
        text += std::to_string(static_cast<long long>(values->entries[i].second));
      }
    } break;
    case StyioDictValueKind::F64: {
      auto* values = static_cast<StyioDictF64*>(dict);
      for (size_t i = 0; i < values->entries.size(); ++i) {
        if (i > 0) {
          text.push_back(',');
        }
        text.push_back('"');
        text += escape_string_for_literal(values->entries[i].first);
        text += "\":";
        text += format_f64_literal(values->entries[i].second);
      }
    } break;
    case StyioDictValueKind::String: {
      auto* values = static_cast<StyioDictString*>(dict);
      for (size_t i = 0; i < values->entries.size(); ++i) {
        if (i > 0) {
          text.push_back(',');
        }
        text.push_back('"');
        text += escape_string_for_literal(values->entries[i].first);
        text += "\":\"";
        text += escape_string_for_literal(values->entries[i].second);
        text.push_back('"');
      }
    } break;
    case StyioDictValueKind::ListHandle: {
      auto* values = static_cast<StyioDictListHandle*>(dict);
      for (size_t i = 0; i < values->entries.size(); ++i) {
        if (i > 0) {
          text.push_back(',');
        }
        text.push_back('"');
        text += escape_string_for_literal(values->entries[i].first);
        text += "\":";
        append_list_handle_repr(text, values->entries[i].second);
      }
    } break;
    case StyioDictValueKind::DictHandle: {
      auto* values = static_cast<StyioDictDictHandle*>(dict);
      for (size_t i = 0; i < values->entries.size(); ++i) {
        if (i > 0) {
          text.push_back(',');
        }
        text.push_back('"');
        text += escape_string_for_literal(values->entries[i].first);
        text += "\":";
        append_dict_handle_repr(text, values->entries[i].second);
      }
    } break;
  }
  text.push_back('}');
  out += text;
}

}  // namespace

extern "C" DLLEXPORT int64_t
styio_file_open(const char* path) {
  if (path == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeFilePathNull, "file path is null");
    return 0;
  }
  std::string resolved = resolve_read_path(path);
  FILE* f = std::fopen(resolved.c_str(), "rb");
  if (f == nullptr) {
    set_runtime_error_once(
      kRuntimeSubcodeFileOpenRead,
      std::string("cannot open file for read: ") + path);
    return 0;
  }
  return stash_file(f);
}

extern "C" DLLEXPORT int64_t
styio_file_open_auto(const char* path) {
  /* M5: same as explicit file path for local filesystem paths. */
  return styio_file_open(path);
}

extern "C" DLLEXPORT int64_t
styio_file_open_write(const char* path) {
  if (path == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeFilePathNull, "file path is null");
    return 0;
  }
  /* Append so repeated writes in one program (e.g. per-iteration << file) accumulate. */
  FILE* f = std::fopen(path, "ab");
  if (f == nullptr) {
    set_runtime_error_once(
      kRuntimeSubcodeFileOpenWrite,
      std::string("cannot open file for write: ") + path);
    return 0;
  }
  return stash_file(f);
}

extern "C" DLLEXPORT void
styio_file_close(int64_t h) {
  (void)g_handle_table.release(h, StyioHandleTable::HandleKind::File, close_file);
}

extern "C" DLLEXPORT void
styio_file_rewind(int64_t h) {
  FILE* f = as_file(h, true);
  if (f != nullptr) {
    std::rewind(f);
  }
}

extern "C" DLLEXPORT const char*
styio_file_read_line(int64_t h) {
  FILE* f = as_file(h, true);
  if (f == nullptr) {
    return nullptr;
  }
  int w = g_read_line_buf_which;
  g_read_line_buf_which = 1 - g_read_line_buf_which;
  char* buf = g_read_line_bufs[w];
  if (std::fgets(buf, static_cast<int>(sizeof(g_read_line_bufs[0])), f) == nullptr) {
    return nullptr;
  }
  size_t n = std::strlen(buf);
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
    buf[--n] = '\0';
  }
  return buf;
}

extern "C" DLLEXPORT int64_t
styio_cstr_to_i64(const char* s) {
  if (s == nullptr || s[0] == '\0') {
    set_runtime_error_once(kRuntimeSubcodeNumericParse, "cannot parse empty string as integer");
    return 0;
  }
  char* end = nullptr;
  errno = 0;
  long long value = std::strtoll(s, &end, 10);
  if (end == s || *end != '\0' || errno == ERANGE) {
    set_runtime_error_once(
      kRuntimeSubcodeNumericParse,
      std::string("cannot parse integer from string: '") + s + "'");
    return 0;
  }
  return static_cast<int64_t>(value);
}

extern "C" DLLEXPORT double
styio_cstr_to_f64(const char* s) {
  if (s == nullptr || s[0] == '\0') {
    set_runtime_error_once(kRuntimeSubcodeNumericParse, "cannot parse empty string as float");
    return 0.0;
  }
  char* end = nullptr;
  errno = 0;
  double value = std::strtod(s, &end);
  if (end == s || *end != '\0' || errno == ERANGE) {
    set_runtime_error_once(
      kRuntimeSubcodeNumericParse,
      std::string("cannot parse float from string: '") + s + "'");
    return 0.0;
  }
  return value;
}

extern "C" DLLEXPORT void
styio_file_write_cstr(int64_t h, const char* data) {
  FILE* f = as_file(h, true);
  if (f == nullptr || data == nullptr) {
    return;
  }
  std::fputs(data, f);
}

extern "C" DLLEXPORT int64_t
styio_read_file_i64line(const char* path) {
  if (path == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeFilePathNull, "file path is null");
    return 0;
  }
  std::string resolved = resolve_read_path(path);
  FILE* f = std::fopen(resolved.c_str(), "rb");
  if (f == nullptr) {
    set_runtime_error_once(
      kRuntimeSubcodeFileOpenRead,
      std::string("cannot open file for read: ") + path);
    return 0;
  }
  char buf[512];
  if (std::fgets(buf, static_cast<int>(sizeof(buf)), f) == nullptr) {
    std::fclose(f);
    return 0;
  }
  std::fclose(f);
  size_t n = std::strlen(buf);
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
    buf[--n] = '\0';
  }
  return styio_cstr_to_i64(buf);
}

extern "C" DLLEXPORT const char*
styio_strcat_ab(const char* a, const char* b) {
  const char* ea = a ? a : "";
  const char* eb = b ? b : "";
  size_t na = std::strlen(ea);
  size_t nb = std::strlen(eb);
  char* p = static_cast<char*>(std::malloc(na + nb + 1));
  if (p == nullptr) {
    return "";
  }
  std::memcpy(p, ea, na);
  std::memcpy(p + na, eb, nb + 1);
  g_owned_cstr_ptrs.insert(p);
  return p;
}

extern "C" DLLEXPORT void
styio_free_cstr(const char* s) {
  if (s == nullptr) {
    return;
  }
  auto it = g_owned_cstr_ptrs.find(s);
  if (it == g_owned_cstr_ptrs.end()) {
    return;
  }
  g_owned_cstr_ptrs.erase(it);
  std::free(const_cast<char*>(s));
}

thread_local char g_i64_dec_buf[32];
thread_local char g_f64_dec_buf[64];

extern "C" DLLEXPORT const char*
styio_i64_dec_cstr(int64_t v) {
  std::snprintf(
    g_i64_dec_buf,
    sizeof(g_i64_dec_buf),
    "%lld",
    static_cast<long long>(v));
  return g_i64_dec_buf;
}

extern "C" DLLEXPORT const char*
styio_f64_dec_cstr(double v) {
  std::snprintf(
    g_f64_dec_buf,
    sizeof(g_f64_dec_buf),
    "%.6f",
    v);
  return g_f64_dec_buf;
}

extern "C" DLLEXPORT int
styio_runtime_has_error() {
  return g_runtime_error ? 1 : 0;
}

extern "C" DLLEXPORT const char*
styio_runtime_last_error() {
  if (!g_runtime_error || g_runtime_error_message.empty()) {
    return nullptr;
  }
  return g_runtime_error_message.c_str();
}

extern "C" DLLEXPORT const char*
styio_runtime_last_error_subcode() {
  if (!g_runtime_error || g_runtime_error_subcode.empty()) {
    return nullptr;
  }
  return g_runtime_error_subcode.c_str();
}

extern "C" DLLEXPORT void
styio_runtime_clear_error() {
  g_runtime_error = false;
  g_runtime_error_message.clear();
  g_runtime_error_subcode.clear();
}

/* M9: write a C-string to stderr with trailing newline and immediate flush.
   Null-safe (no-op for nullptr). */
extern "C" DLLEXPORT void
styio_stderr_write_cstr(const char* s) {
  if (s != nullptr) {
    std::fprintf(stderr, "%s\n", s);
    std::fflush(stderr);
  }
}

/* M10: read one line from stdin into a thread-local buffer.
   Returns borrowed pointer (valid until next call on this thread).
   Returns nullptr on EOF. Strips trailing newline/CR. */
thread_local char g_stdin_line_buf[65536];

extern "C" DLLEXPORT const char*
styio_stdin_read_line() {
  if (std::fgets(g_stdin_line_buf, static_cast<int>(sizeof(g_stdin_line_buf)), stdin) == nullptr) {
    return nullptr;
  }
  size_t n = std::strlen(g_stdin_line_buf);
  while (n > 0 && (g_stdin_line_buf[n - 1] == '\n' || g_stdin_line_buf[n - 1] == '\r')) {
    g_stdin_line_buf[--n] = '\0';
  }
  return g_stdin_line_buf;
}

extern "C" DLLEXPORT int64_t
styio_list_i64_read_stdin() {
  std::vector<int64_t> values;
  std::string input = read_all_stdin();
  if (!parse_i64_list_literal(input, values)) {
    set_runtime_error_once(
      kRuntimeSubcodeListParse,
      "stdin does not contain a valid Styio list literal");
    return 0;
  }
  auto* list = new StyioListI64();
  list->elems = std::move(values);
  return stash_list(list);
}

extern "C" DLLEXPORT int64_t
styio_list_cstr_read_stdin() {
  auto* list = new StyioListString();
  char line_buf[65536];
  while (std::fgets(line_buf, static_cast<int>(sizeof(line_buf)), stdin) != nullptr) {
    size_t n = std::strlen(line_buf);
    while (n > 0 && (line_buf[n - 1] == '\n' || line_buf[n - 1] == '\r')) {
      line_buf[--n] = '\0';
    }
    list->elems.emplace_back(line_buf);
  }
  return stash_list(list);
}

extern "C" DLLEXPORT int64_t
styio_list_new_bool() {
  return stash_list(new StyioListBool());
}

extern "C" DLLEXPORT int64_t
styio_list_new_i64() {
  return stash_list(new StyioListI64());
}

extern "C" DLLEXPORT int64_t
styio_list_new_f64() {
  return stash_list(new StyioListF64());
}

extern "C" DLLEXPORT int64_t
styio_list_new_cstr() {
  return stash_list(new StyioListString());
}

extern "C" DLLEXPORT int64_t
styio_list_new_list() {
  return stash_list(new StyioListListHandle());
}

extern "C" DLLEXPORT int64_t
styio_list_new_dict() {
  return stash_list(new StyioListDictHandle());
}

extern "C" DLLEXPORT void
styio_list_push_bool(int64_t h, int64_t value) {
  StyioListBool* list = as_list_bool(h, true);
  if (list != nullptr) {
    list->elems.push_back(value != 0 ? 1 : 0);
  }
}

extern "C" DLLEXPORT void
styio_list_push_i64(int64_t h, int64_t value) {
  StyioListI64* list = as_list_i64(h, true);
  if (list != nullptr) {
    list->elems.push_back(value);
  }
}

extern "C" DLLEXPORT void
styio_list_push_f64(int64_t h, double value) {
  StyioListF64* list = as_list_f64(h, true);
  if (list != nullptr) {
    list->elems.push_back(value);
  }
}

extern "C" DLLEXPORT void
styio_list_push_cstr(int64_t h, const char* value) {
  StyioListString* list = as_list_string(h, true);
  if (list != nullptr) {
    list->elems.emplace_back(value == nullptr ? "" : value);
  }
}

extern "C" DLLEXPORT void
styio_list_push_list(int64_t h, int64_t value) {
  StyioListListHandle* list = as_list_list_handle(h, true);
  if (list != nullptr) {
    list->elems.push_back(clone_list_handle_value(value));
  }
}

extern "C" DLLEXPORT void
styio_list_push_dict(int64_t h, int64_t value) {
  StyioListDictHandle* list = as_list_dict_handle(h, true);
  if (list != nullptr) {
    list->elems.push_back(clone_dict_handle_value(value));
  }
}

extern "C" DLLEXPORT int64_t
styio_list_clone(int64_t h) {
  return clone_list_handle_value(h);
}

extern "C" DLLEXPORT int64_t
styio_list_len(int64_t h) {
  StyioListBase* list = as_list_base(h, true);
  if (list == nullptr) {
    return 0;
  }
  if (list->elem_kind == StyioListElemKind::Bool) {
    return static_cast<int64_t>(static_cast<StyioListBool*>(list)->elems.size());
  }
  if (list->elem_kind == StyioListElemKind::I64) {
    return static_cast<int64_t>(static_cast<StyioListI64*>(list)->elems.size());
  }
  if (list->elem_kind == StyioListElemKind::F64) {
    return static_cast<int64_t>(static_cast<StyioListF64*>(list)->elems.size());
  }
  if (list->elem_kind == StyioListElemKind::String) {
    return static_cast<int64_t>(static_cast<StyioListString*>(list)->elems.size());
  }
  if (list->elem_kind == StyioListElemKind::ListHandle) {
    return static_cast<int64_t>(static_cast<StyioListListHandle*>(list)->elems.size());
  }
  return static_cast<int64_t>(static_cast<StyioListDictHandle*>(list)->elems.size());
}

extern "C" DLLEXPORT int64_t
styio_list_get_bool(int64_t h, int64_t idx) {
  StyioListBool* list = as_list_bool(h, true);
  if (list == nullptr) {
    return 0;
  }
  if (idx < 0 || static_cast<size_t>(idx) >= list->elems.size()) {
    set_runtime_error_once(
      kRuntimeSubcodeListIndex,
      "list index out of range: " + std::to_string(static_cast<long long>(idx)));
    return 0;
  }
  return list->elems[static_cast<size_t>(idx)] != 0 ? 1 : 0;
}

extern "C" DLLEXPORT int64_t
styio_list_get(int64_t h, int64_t idx) {
  StyioListI64* list = as_list_i64(h, true);
  if (list == nullptr) {
    return 0;
  }
  if (idx < 0 || static_cast<size_t>(idx) >= list->elems.size()) {
    set_runtime_error_once(
      kRuntimeSubcodeListIndex,
      "list index out of range: " + std::to_string(static_cast<long long>(idx)));
    return 0;
  }
  return list->elems[static_cast<size_t>(idx)];
}

extern "C" DLLEXPORT double
styio_list_get_f64(int64_t h, int64_t idx) {
  StyioListF64* list = as_list_f64(h, true);
  if (list == nullptr) {
    return 0.0;
  }
  if (idx < 0 || static_cast<size_t>(idx) >= list->elems.size()) {
    set_runtime_error_once(
      kRuntimeSubcodeListIndex,
      "list index out of range: " + std::to_string(static_cast<long long>(idx)));
    return 0.0;
  }
  return list->elems[static_cast<size_t>(idx)];
}

extern "C" DLLEXPORT const char*
styio_list_get_cstr(int64_t h, int64_t idx) {
  StyioListString* list = as_list_string(h, true);
  if (list == nullptr) {
    return nullptr;
  }
  if (idx < 0 || static_cast<size_t>(idx) >= list->elems.size()) {
    set_runtime_error_once(
      kRuntimeSubcodeListIndex,
      "list index out of range: " + std::to_string(static_cast<long long>(idx)));
    return nullptr;
  }
  return copy_to_owned_cstr(list->elems[static_cast<size_t>(idx)]);
}

extern "C" DLLEXPORT int64_t
styio_list_get_list(int64_t h, int64_t idx) {
  StyioListListHandle* list = as_list_list_handle(h, true);
  if (list == nullptr) {
    return 0;
  }
  if (idx < 0 || static_cast<size_t>(idx) >= list->elems.size()) {
    set_runtime_error_once(
      kRuntimeSubcodeListIndex,
      "list index out of range: " + std::to_string(static_cast<long long>(idx)));
    return 0;
  }
  return clone_list_handle_value(list->elems[static_cast<size_t>(idx)]);
}

extern "C" DLLEXPORT int64_t
styio_list_get_dict(int64_t h, int64_t idx) {
  StyioListDictHandle* list = as_list_dict_handle(h, true);
  if (list == nullptr) {
    return 0;
  }
  if (idx < 0 || static_cast<size_t>(idx) >= list->elems.size()) {
    set_runtime_error_once(
      kRuntimeSubcodeListIndex,
      "list index out of range: " + std::to_string(static_cast<long long>(idx)));
    return 0;
  }
  return clone_dict_handle_value(list->elems[static_cast<size_t>(idx)]);
}

extern "C" DLLEXPORT void
styio_list_set(int64_t h, int64_t idx, int64_t value) {
  StyioListI64* list = as_list_i64(h, true);
  if (list == nullptr) {
    return;
  }
  if (idx < 0 || static_cast<size_t>(idx) >= list->elems.size()) {
    set_runtime_error_once(
      kRuntimeSubcodeListIndex,
      "list index out of range: " + std::to_string(static_cast<long long>(idx)));
    return;
  }
  list->elems[static_cast<size_t>(idx)] = value;
}

extern "C" DLLEXPORT const char*
styio_list_to_cstr(int64_t h) {
  StyioListBase* list = as_list_base(h, true);
  if (list == nullptr) {
    return nullptr;
  }
  std::string text;
  append_list_handle_repr(text, h);
  return copy_to_owned_cstr(text);
}

extern "C" DLLEXPORT void
styio_list_release(int64_t h) {
  (void)g_handle_table.release(h, StyioHandleTable::HandleKind::List, close_list);
}

extern "C" DLLEXPORT int64_t
styio_list_active_count() {
  return g_active_list_handles;
}

extern "C" DLLEXPORT int64_t
styio_dict_new_bool() {
  return stash_dict(make_dict_storage_for_current_backend<StyioDictBool>());
}

extern "C" DLLEXPORT int64_t
styio_dict_new_i64() {
  return stash_dict(make_dict_storage_for_current_backend<StyioDictI64>());
}

extern "C" DLLEXPORT int64_t
styio_dict_new_f64() {
  return stash_dict(make_dict_storage_for_current_backend<StyioDictF64>());
}

extern "C" DLLEXPORT int64_t
styio_dict_new_cstr() {
  return stash_dict(make_dict_storage_for_current_backend<StyioDictString>());
}

extern "C" DLLEXPORT int64_t
styio_dict_new_list() {
  return stash_dict(make_dict_storage_for_current_backend<StyioDictListHandle>());
}

extern "C" DLLEXPORT int64_t
styio_dict_new_dict() {
  return stash_dict(make_dict_storage_for_current_backend<StyioDictDictHandle>());
}

extern "C" DLLEXPORT int64_t
styio_dict_clone(int64_t h) {
  return clone_dict_handle_value(h);
}

extern "C" DLLEXPORT int64_t
styio_dict_len(int64_t h) {
  StyioDictBase* dict = as_dict_base(h, true);
  if (dict == nullptr) {
    return 0;
  }
  switch (dict->value_kind) {
    case StyioDictValueKind::Bool:
      return static_cast<int64_t>(static_cast<StyioDictBool*>(dict)->entries.size());
    case StyioDictValueKind::I64:
      return static_cast<int64_t>(static_cast<StyioDictI64*>(dict)->entries.size());
    case StyioDictValueKind::F64:
      return static_cast<int64_t>(static_cast<StyioDictF64*>(dict)->entries.size());
    case StyioDictValueKind::String:
      return static_cast<int64_t>(static_cast<StyioDictString*>(dict)->entries.size());
    case StyioDictValueKind::ListHandle:
      return static_cast<int64_t>(static_cast<StyioDictListHandle*>(dict)->entries.size());
    case StyioDictValueKind::DictHandle:
      return static_cast<int64_t>(static_cast<StyioDictDictHandle*>(dict)->entries.size());
  }
  return 0;
}

extern "C" DLLEXPORT int64_t
styio_dict_get_bool(int64_t h, const char* key) {
  StyioDictBool* dict = as_dict_bool(h, true);
  if (dict == nullptr) {
    return 0;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return 0;
  }
  int64_t value = 0;
  if (dict_try_get(dict, key, value)) {
    return value != 0 ? 1 : 0;
  }
  set_runtime_error_once(
    kRuntimeSubcodeDictKey,
    std::string("dict key not found: ") + key);
  return 0;
}

extern "C" DLLEXPORT int64_t
styio_dict_get_i64(int64_t h, const char* key) {
  StyioDictI64* dict = as_dict_i64(h, true);
  if (dict == nullptr) {
    return 0;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return 0;
  }
  int64_t value = 0;
  if (dict_try_get(dict, key, value)) {
    return value;
  }
  set_runtime_error_once(
    kRuntimeSubcodeDictKey,
    std::string("dict key not found: ") + key);
  return 0;
}

extern "C" DLLEXPORT double
styio_dict_get_f64(int64_t h, const char* key) {
  StyioDictF64* dict = as_dict_f64(h, true);
  if (dict == nullptr) {
    return 0.0;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return 0.0;
  }
  double value = 0.0;
  if (dict_try_get(dict, key, value)) {
    return value;
  }
  set_runtime_error_once(
    kRuntimeSubcodeDictKey,
    std::string("dict key not found: ") + key);
  return 0.0;
}

extern "C" DLLEXPORT const char*
styio_dict_get_cstr(int64_t h, const char* key) {
  StyioDictString* dict = as_dict_string(h, true);
  if (dict == nullptr) {
    return nullptr;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return nullptr;
  }
  std::string value;
  if (dict_try_get(dict, key, value)) {
    return copy_to_owned_cstr(value);
  }
  set_runtime_error_once(
    kRuntimeSubcodeDictKey,
    std::string("dict key not found: ") + key);
  return nullptr;
}

extern "C" DLLEXPORT int64_t
styio_dict_get_list(int64_t h, const char* key) {
  StyioDictListHandle* dict = as_dict_list(h, true);
  if (dict == nullptr) {
    return 0;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return 0;
  }
  int64_t value = 0;
  if (dict_try_get(dict, key, value)) {
    return clone_list_handle_value(value);
  }
  set_runtime_error_once(
    kRuntimeSubcodeDictKey,
    std::string("dict key not found: ") + key);
  return 0;
}

extern "C" DLLEXPORT int64_t
styio_dict_get_dict(int64_t h, const char* key) {
  StyioDictDictHandle* dict = as_dict_dict(h, true);
  if (dict == nullptr) {
    return 0;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return 0;
  }
  int64_t value = 0;
  if (dict_try_get(dict, key, value)) {
    return clone_dict_handle_value(value);
  }
  set_runtime_error_once(
    kRuntimeSubcodeDictKey,
    std::string("dict key not found: ") + key);
  return 0;
}

extern "C" DLLEXPORT void
styio_dict_set_bool(int64_t h, const char* key, int64_t value) {
  StyioDictBool* dict = as_dict_bool(h, true);
  if (dict == nullptr) {
    return;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return;
  }
  dict_set(dict, key, value != 0 ? 1 : 0);
}

extern "C" DLLEXPORT void
styio_dict_set_i64(int64_t h, const char* key, int64_t value) {
  StyioDictI64* dict = as_dict_i64(h, true);
  if (dict == nullptr) {
    return;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return;
  }
  dict_set(dict, key, value);
}

extern "C" DLLEXPORT void
styio_dict_set_f64(int64_t h, const char* key, double value) {
  StyioDictF64* dict = as_dict_f64(h, true);
  if (dict == nullptr) {
    return;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return;
  }
  dict_set(dict, key, value);
}

extern "C" DLLEXPORT void
styio_dict_set_cstr(int64_t h, const char* key, const char* value) {
  StyioDictString* dict = as_dict_string(h, true);
  if (dict == nullptr) {
    return;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return;
  }
  const std::string stored = value == nullptr ? std::string() : std::string(value);
  dict_set(dict, key, stored);
}

extern "C" DLLEXPORT void
styio_dict_set_list(int64_t h, const char* key, int64_t value) {
  StyioDictListHandle* dict = as_dict_list(h, true);
  if (dict == nullptr) {
    return;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return;
  }
  int64_t stored = clone_list_handle_value(value);
  dict_set_handle(
    dict,
    key,
    stored,
    [](int64_t existing) {
      (void)g_handle_table.release(existing, StyioHandleTable::HandleKind::List, close_list);
    });
}

extern "C" DLLEXPORT void
styio_dict_set_dict(int64_t h, const char* key, int64_t value) {
  StyioDictDictHandle* dict = as_dict_dict(h, true);
  if (dict == nullptr) {
    return;
  }
  if (key == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeDictKey, "dict key is null");
    return;
  }
  int64_t stored = clone_dict_handle_value(value);
  dict_set_handle(
    dict,
    key,
    stored,
    [](int64_t existing) {
      (void)g_handle_table.release(existing, StyioHandleTable::HandleKind::Dict, close_dict);
    });
}

extern "C" DLLEXPORT int64_t
styio_dict_keys(int64_t h) {
  StyioDictBase* dict = as_dict_base(h, true);
  if (dict == nullptr) {
    return 0;
  }
  auto* keys = new StyioListString();
  switch (dict->value_kind) {
    case StyioDictValueKind::Bool:
      keys->elems.reserve(static_cast<StyioDictBool*>(dict)->entries.size());
      for (const auto& entry : static_cast<StyioDictBool*>(dict)->entries) {
        keys->elems.push_back(entry.first);
      }
      break;
    case StyioDictValueKind::I64:
      keys->elems.reserve(static_cast<StyioDictI64*>(dict)->entries.size());
      for (const auto& entry : static_cast<StyioDictI64*>(dict)->entries) {
        keys->elems.push_back(entry.first);
      }
      break;
    case StyioDictValueKind::F64:
      keys->elems.reserve(static_cast<StyioDictF64*>(dict)->entries.size());
      for (const auto& entry : static_cast<StyioDictF64*>(dict)->entries) {
        keys->elems.push_back(entry.first);
      }
      break;
    case StyioDictValueKind::String:
      keys->elems.reserve(static_cast<StyioDictString*>(dict)->entries.size());
      for (const auto& entry : static_cast<StyioDictString*>(dict)->entries) {
        keys->elems.push_back(entry.first);
      }
      break;
    case StyioDictValueKind::ListHandle:
      keys->elems.reserve(static_cast<StyioDictListHandle*>(dict)->entries.size());
      for (const auto& entry : static_cast<StyioDictListHandle*>(dict)->entries) {
        keys->elems.push_back(entry.first);
      }
      break;
    case StyioDictValueKind::DictHandle:
      keys->elems.reserve(static_cast<StyioDictDictHandle*>(dict)->entries.size());
      for (const auto& entry : static_cast<StyioDictDictHandle*>(dict)->entries) {
        keys->elems.push_back(entry.first);
      }
      break;
  }
  return stash_list(keys);
}

extern "C" DLLEXPORT int64_t
styio_dict_values_bool(int64_t h) {
  StyioDictBool* dict = as_dict_bool(h, true);
  if (dict == nullptr) {
    return 0;
  }
  auto* values = new StyioListBool();
  values->elems.reserve(dict->entries.size());
  for (const auto& entry : dict->entries) {
    values->elems.push_back(entry.second);
  }
  return stash_list(values);
}

extern "C" DLLEXPORT int64_t
styio_dict_values_i64(int64_t h) {
  StyioDictI64* dict = as_dict_i64(h, true);
  if (dict == nullptr) {
    return 0;
  }
  auto* values = new StyioListI64();
  values->elems.reserve(dict->entries.size());
  for (const auto& entry : dict->entries) {
    values->elems.push_back(entry.second);
  }
  return stash_list(values);
}

extern "C" DLLEXPORT int64_t
styio_dict_values_f64(int64_t h) {
  StyioDictF64* dict = as_dict_f64(h, true);
  if (dict == nullptr) {
    return 0;
  }
  auto* values = new StyioListF64();
  values->elems.reserve(dict->entries.size());
  for (const auto& entry : dict->entries) {
    values->elems.push_back(entry.second);
  }
  return stash_list(values);
}

extern "C" DLLEXPORT int64_t
styio_dict_values_cstr(int64_t h) {
  StyioDictString* dict = as_dict_string(h, true);
  if (dict == nullptr) {
    return 0;
  }
  auto* values = new StyioListString();
  values->elems.reserve(dict->entries.size());
  for (const auto& entry : dict->entries) {
    values->elems.push_back(entry.second);
  }
  return stash_list(values);
}

extern "C" DLLEXPORT int64_t
styio_dict_values_list(int64_t h) {
  StyioDictListHandle* dict = as_dict_list(h, true);
  if (dict == nullptr) {
    return 0;
  }
  auto* values = new StyioListListHandle();
  values->elems.reserve(dict->entries.size());
  for (const auto& entry : dict->entries) {
    values->elems.push_back(clone_list_handle_value(entry.second));
  }
  return stash_list(values);
}

extern "C" DLLEXPORT int64_t
styio_dict_values_dict(int64_t h) {
  StyioDictDictHandle* dict = as_dict_dict(h, true);
  if (dict == nullptr) {
    return 0;
  }
  auto* values = new StyioListDictHandle();
  values->elems.reserve(dict->entries.size());
  for (const auto& entry : dict->entries) {
    values->elems.push_back(clone_dict_handle_value(entry.second));
  }
  return stash_list(values);
}

extern "C" DLLEXPORT const char*
styio_dict_to_cstr(int64_t h) {
  StyioDictBase* dict = as_dict_base(h, true);
  if (dict == nullptr) {
    return nullptr;
  }
  std::string text;
  append_dict_handle_repr(text, h);
  return copy_to_owned_cstr(text);
}

extern "C" DLLEXPORT void
styio_dict_release(int64_t h) {
  (void)g_handle_table.release(h, StyioHandleTable::HandleKind::Dict, close_dict);
}

extern "C" DLLEXPORT int64_t
styio_dict_active_count() {
  return g_active_dict_handles;
}

extern "C" DLLEXPORT int
styio_dict_runtime_supported_impl_count() {
  return kStyioDictBackendRegistryCount;
}

extern "C" DLLEXPORT const char*
styio_dict_runtime_supported_impl_name(int index) {
  if (index < 0 || index >= kStyioDictBackendRegistryCount) {
    return nullptr;
  }
  return kStyioDictBackendRegistry[index].name;
}

extern "C" DLLEXPORT const char*
styio_dict_runtime_canonical_impl_name(const char* raw_name) {
  const auto* spec = dict_backend_spec_by_name(raw_name);
  if (spec == nullptr) {
    return nullptr;
  }
  return spec->name;
}

extern "C" DLLEXPORT int
styio_dict_runtime_set_impl_by_name(const char* raw_name) {
  const auto* spec = dict_backend_spec_by_name(raw_name);
  if (spec == nullptr) {
    return 0;
  }
  g_default_dict_runtime_impl = spec->impl;
  return 1;
}

extern "C" DLLEXPORT const char*
styio_dict_runtime_get_impl_name() {
  return current_dict_backend_spec()->name;
}

extern "C" DLLEXPORT int
styio_dict_runtime_set_impl(int impl) {
  const auto* spec = dict_backend_spec_by_impl(static_cast<StyioDictRuntimeImpl>(impl));
  if (spec == nullptr) {
    return 0;
  }
  g_default_dict_runtime_impl = spec->impl;
  return 1;
}

extern "C" DLLEXPORT int
styio_dict_runtime_get_impl() {
  return static_cast<int>(g_default_dict_runtime_impl);
}

extern "C" DLLEXPORT int
something() {
  return 0;
}
