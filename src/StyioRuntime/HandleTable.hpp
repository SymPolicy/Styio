#pragma once
#ifndef STYIO_RUNTIME_HANDLE_TABLE_HPP_
#define STYIO_RUNTIME_HANDLE_TABLE_HPP_

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

class StyioHandleTable
{
public:
  using HandleId = int64_t;

  enum class HandleKind
  {
    File = 0,
    Resource = 1,
    List = 2,
    Dict = 3,
    Matrix = 4,
    Task = 5,
    Unknown = 6,
  };

  struct Entry
  {
    HandleKind kind = HandleKind::Unknown;
    void* ptr = nullptr;
    bool valid = false;
  };

private:
  std::vector<Entry> entries_;
  std::vector<HandleId> free_handles_;

  static bool is_indexable(HandleId id) {
    return id > 0;
  }

  Entry* entry_for(HandleId id) {
    if (!is_indexable(id)) {
      return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(id - 1);
    if (index >= entries_.size()) {
      return nullptr;
    }
    return &entries_[index];
  }

  const Entry* entry_for(HandleId id) const {
    if (!is_indexable(id)) {
      return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(id - 1);
    if (index >= entries_.size()) {
      return nullptr;
    }
    return &entries_[index];
  }

  static bool occupied(const Entry& entry) {
    return entry.kind != HandleKind::Unknown;
  }

  void recycle_handle(HandleId id, Entry* entry) {
    if (entry == nullptr || !occupied(*entry)) {
      return;
    }
    *entry = Entry{};
    free_handles_.push_back(id);
  }

public:
  HandleId acquire(HandleKind kind, void* ptr) {
    if (ptr == nullptr) {
      return 0;
    }
    HandleId id = 0;
    if (!free_handles_.empty()) {
      id = free_handles_.back();
      free_handles_.pop_back();
      entries_[static_cast<std::size_t>(id - 1)] = Entry{kind, ptr, true};
    }
    else {
      entries_.push_back(Entry{kind, ptr, true});
      id = static_cast<HandleId>(entries_.size());
    }
    return id;
  }

  void* lookup(HandleId id, HandleKind expected_kind = HandleKind::Unknown) const {
    const Entry* entry = entry_for(id);
    if (entry == nullptr || !entry->valid) {
      return nullptr;
    }
    if (expected_kind != HandleKind::Unknown && entry->kind != expected_kind) {
      return nullptr;
    }
    return entry->ptr;
  }

  template <typename T>
  T* lookup_as(HandleId id, HandleKind expected_kind = HandleKind::Unknown) const {
    return static_cast<T*>(lookup(id, expected_kind));
  }

  template <typename Closer>
  bool release(HandleId id, HandleKind expected_kind, Closer&& closer) {
    Entry* entry = entry_for(id);
    if (entry == nullptr || !entry->valid) {
      return false;
    }
    if (expected_kind != HandleKind::Unknown && entry->kind != expected_kind) {
      return false;
    }
    void* raw = entry->ptr;
    recycle_handle(id, entry);
    if (raw != nullptr) {
      std::forward<Closer>(closer)(raw);
    }
    return true;
  }

  bool release(HandleId id, HandleKind expected_kind = HandleKind::Unknown) {
    return release(id, expected_kind, [](void*) {});
  }

  template <typename Closer>
  size_t release_all(HandleKind expected_kind, Closer&& closer) {
    size_t released = 0;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      Entry& entry = entries_[index];
      if (!occupied(entry)) {
        continue;
      }
      if (expected_kind != HandleKind::Unknown && entry.kind != expected_kind) {
        continue;
      }
      if (!entry.valid) {
        recycle_handle(static_cast<HandleId>(index + 1), &entry);
        continue;
      }
      void* raw = entry.ptr;
      recycle_handle(static_cast<HandleId>(index + 1), &entry);
      if (raw != nullptr) {
        std::forward<Closer>(closer)(raw);
      }
      ++released;
    }
    return released;
  }

  HandleId reserve_stub(HandleKind kind) {
    HandleId id = 0;
    if (!free_handles_.empty()) {
      id = free_handles_.back();
      free_handles_.pop_back();
      entries_[static_cast<std::size_t>(id - 1)] = Entry{kind, nullptr, false};
    }
    else {
      entries_.push_back(Entry{kind, nullptr, false});
      id = static_cast<HandleId>(entries_.size());
    }
    return id;
  }

  bool contains(HandleId id) const {
    const Entry* entry = entry_for(id);
    return entry != nullptr && occupied(*entry);
  }

  void invalidate(HandleId id) {
    recycle_handle(id, entry_for(id));
  }

  size_t size() const {
    size_t count = 0;
    for (const Entry& entry : entries_) {
      if (occupied(entry)) {
        ++count;
      }
    }
    return count;
  }
};

#endif // STYIO_RUNTIME_HANDLE_TABLE_HPP_
