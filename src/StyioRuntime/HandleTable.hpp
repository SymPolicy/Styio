#pragma once
#ifndef STYIO_RUNTIME_HANDLE_TABLE_HPP_
#define STYIO_RUNTIME_HANDLE_TABLE_HPP_

#include <cstddef>
#include <cstdint>
#include <utility>
#include <unordered_map>

class StyioHandleTable
{
public:
  using HandleId = int64_t;

  enum class HandleKind
  {
    File = 0,
    Resource = 1,
    Unknown = 2,
  };

  struct Entry
  {
    HandleKind kind = HandleKind::Unknown;
    void* ptr = nullptr;
    bool valid = false;
  };

private:
  std::unordered_map<HandleId, Entry> entries_;
  HandleId next_handle_ = 1;

public:
  HandleId acquire(HandleKind kind, void* ptr) {
    if (ptr == nullptr) {
      return 0;
    }
    const HandleId id = next_handle_++;
    entries_[id] = Entry{kind, ptr, true};
    return id;
  }

  void* lookup(HandleId id, HandleKind expected_kind = HandleKind::Unknown) const {
    auto it = entries_.find(id);
    if (it == entries_.end() || !it->second.valid) {
      return nullptr;
    }
    if (expected_kind != HandleKind::Unknown && it->second.kind != expected_kind) {
      return nullptr;
    }
    return it->second.ptr;
  }

  template <typename T>
  T* lookup_as(HandleId id, HandleKind expected_kind = HandleKind::Unknown) const {
    return static_cast<T*>(lookup(id, expected_kind));
  }

  template <typename Closer>
  bool release(HandleId id, HandleKind expected_kind, Closer&& closer) {
    auto it = entries_.find(id);
    if (it == entries_.end() || !it->second.valid) {
      return false;
    }
    if (expected_kind != HandleKind::Unknown && it->second.kind != expected_kind) {
      return false;
    }
    void* raw = it->second.ptr;
    entries_.erase(it);
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
    for (auto it = entries_.begin(); it != entries_.end();) {
      if (!it->second.valid) {
        it = entries_.erase(it);
        continue;
      }
      if (expected_kind != HandleKind::Unknown && it->second.kind != expected_kind) {
        ++it;
        continue;
      }
      void* raw = it->second.ptr;
      it = entries_.erase(it);
      if (raw != nullptr) {
        std::forward<Closer>(closer)(raw);
      }
      ++released;
    }
    return released;
  }

  HandleId reserve_stub(HandleKind kind) {
    const HandleId id = next_handle_++;
    entries_[id] = Entry{kind, nullptr, false};
    return id;
  }

  bool contains(HandleId id) const {
    return entries_.find(id) != entries_.end();
  }

  void invalidate(HandleId id) {
    auto it = entries_.find(id);
    if (it != entries_.end()) {
      entries_.erase(it);
    }
  }

  size_t size() const {
    return entries_.size();
  }
};

#endif // STYIO_RUNTIME_HANDLE_TABLE_HPP_
